#pragma once

#include <sw/redis++/redis.h>

#include <array>
#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <concepts>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"

class HeuristicManifest : public Singleton<HeuristicManifest>
{
	Log logger = Log("HeuristicManifest");
	/*
	All available bounds are placed in pending.
	At first or on change. each Shard goes and Pops from the pending list.
	Adding it to Claimed with their key.
	*/
	const std::string HeuristicNamespace = "Heuristic:";

	const std::string HeuristicTypeKey = HeuristicNamespace + "Type";
	const std::string HeuristicTimestampKey = HeuristicNamespace + "Timestamp";
	const std::string HeuristicSerializeDataKey = HeuristicNamespace + "Data";
	// const std::string PendingHashTable =
	//	HeuristicNamespace + "Pending";	 //[BoundID,SerializedBound]

	const std::string PendingIDsSet = HeuristicNamespace + "PendingIDs";
	// const std::string ClaimedHashtable =
	//	HeuristicNamespace + "Claimed";	 //[NetworkIdentity,SerializedBound]
	const std::string BoundID2NIDMap =
		HeuristicNamespace + "BoundID -> NetID Map";  //[BoundID,NetworkIdentity]
	const std::string NID2BoundIDMap =
		HeuristicNamespace + "NetID -> BoundID Map";  //[BoundID,NetworkIdentity]

	IHeuristic::Type ActiveHeuristic = IHeuristic::Type::eNone;
	std::shared_ptr<IHeuristic> CachedHeuristic;
	long long CachedHeuristicTimeStamp = 0;
	std::shared_mutex HeuristicFetchMutex;

   public:
	[[nodiscard]] IHeuristic::Type GetActiveHeuristicType() const;

	std::optional<IBounds::BoundsID> BoundIDFromShard(const NetworkIdentity& id);
	/* template <typename BoundType, typename KeyType = std::string> */
	/* void GetAllPendingBounds(std::vector<BoundType>& out_bounds); */

	/* template <typename BoundType, typename KeyType = std::string> */
	/* void StorePendingBounds(const std::vector<BoundType>& in_bounds); */

	[[nodiscard]] std::optional<IBounds::BoundsID> ClaimNextPendingBound(
		const NetworkIdentity& claim_key);

	[[nodiscard]] long long GetPendingBoundsCount() const;

	[[nodiscard]] long long GetClaimedBoundsCount() const;

	bool ReleaseClaimedBound(IBounds::BoundsID ID);

	template <typename FN>
		requires std::is_invocable_v<FN, const IHeuristic&>
	auto PullHeuristic(FN&& f);
	template <typename FN>
		requires std::is_invocable_v<FN, const IBounds&>
	auto PullHeuristicBound(IBounds::BoundsID ID, FN&& f)
	{
		return PullHeuristic([&](const IHeuristic& h) { return f(h.GetBound(ID)); });
	}
	void PushHeuristic(const IHeuristic& h);

	[[nodiscard]] std::optional<NetworkIdentity> ShardFromPosition(const Transform& t);

	[[nodiscard]] std::optional<NetworkIdentity> ShardFromBoundID(const IBounds::BoundsID id);
	// void StorePendingBound(const IBounds& bound);

   private:
	void Internal_SetActiveHeuristicType(IHeuristic::Type type);
	void Internal_ParseHeuristic(IHeuristic::Type type, std::string_view data);
};

template <typename FN>
	requires std::is_invocable_v<FN, const IHeuristic&>
inline auto HeuristicManifest::PullHeuristic(FN&& f)
{
	const std::optional<std::string> timestamp_s = InternalDB::Get()->Get(HeuristicTimestampKey);
	ASSERT(timestamp_s.has_value(), "INVALID SCENARIO");
	const long long PushTimeStamp = std::stoll(timestamp_s.value());
	if (PushTimeStamp < CachedHeuristicTimeStamp)
	{
		std::shared_lock lock(HeuristicFetchMutex);
		return f(*CachedHeuristic);
	}
	else
	{
		std::unique_lock lock(HeuristicFetchMutex);
		const std::optional<std::string> heuData =
			InternalDB::Get()->Get(HeuristicSerializeDataKey);
		ASSERT(heuData.has_value(), "INVALID SCENARIO");

		const std::optional<std::string> heuType_s = InternalDB::Get()->Get(HeuristicTypeKey);
		ASSERT(heuType_s.has_value(), "INVALID SCENARIO");
		IHeuristic::Type hType;
		IHeuristic::TypeFromString(heuType_s.value(), hType);
		Internal_ParseHeuristic(hType, *heuData);
		CachedHeuristicTimeStamp = PushTimeStamp;
		return f(*CachedHeuristic);
	}
}
/* template <typename BoundType, typename KeyType>
inline void HeuristicManifest::GetAllPendingBounds(std::vector<BoundType>& out_bounds)
{
	out_bounds.clear();
	std::vector<std::string> data_for_readers;
	std::unordered_map<IBounds::BoundsID, ByteReader> brs;
	GetPendingBoundsAsByteReaders(data_for_readers, brs);
	out_bounds.reserve(brs.size());
	for (auto& [bound_id, boundByteReader] : brs)
	{
		out_bounds.emplace_back(BoundType());
		BoundType& b = out_bounds.back();
		b.Deserialize(boundByteReader);
	}
}
template <typename BoundType, typename KeyType>
void HeuristicManifest::StorePendingBounds(const std::vector<BoundType>& in_bounds)
{
	// std::string_view s_b, s_id;
	for (int i = 0; i < in_bounds.size(); i++)
	{
		const auto& bounds = in_bounds[i];

		ByteWriter bw;
		bounds.Serialize(bw);
		// s_b = bw.as_string_view();

		PendingBoundStruct p;
		p.ID = bounds.GetID();
		p.BoundsDataBase64 = bw.as_string_base_64();
		Internal_InsertPendingBound(p);
	}
}

template <typename BoundType>
void HeuristicManifest::GetAllClaimedBounds(
	std::unordered_map<NetworkIdentity, BoundType>& out_bounds)
{
	out_bounds.clear();
	const auto BoundsTable = InternalDB::Get()->HGetAll(ClaimedHashTableNID2BoundData);
	out_bounds.reserve(BoundsTable.size());

	for (auto& [id, boundReaderPair] : claimedReaders)
	{
		auto [it, inserted] = out_bounds.emplace(id, BoundType());
		BoundType& b = it->second;
		b.Deserialize(boundReaderPair.second);
	}
} */