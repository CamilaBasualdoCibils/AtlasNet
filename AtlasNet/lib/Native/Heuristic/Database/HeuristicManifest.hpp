#pragma once

#include <sw/redis++/redis.h>

#include <array>
#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <concepts>
#include <iostream>
#include <memory>
#include <mutex>
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

	const std::string HeuristicTypeKey = "Heuristic_Type";
	const std::string HeuristicTimestampKey = "HeuristicTimestamp";
	const std::string HeuristicSerializeDataKey = "Heuristic_Data";
	/*stores a list of available, unclaimed bounds*/
	/*the bounds id is the shape*/
	// Shared hash tag keeps pending/claimed in the same Redis Cluster slot.
	const std::string PendingHashTable = "{Heuristic_Bounds}:Pending";
	const std::string JSONDataTable = "HeuristicManifest";
	const std::string JSONPendingEntry = "Pending";
	const std::string JSONClaimedEntry = "Claimed";
	const std::string JSONHeuristicData64Entry = "HeuristicData64";
	const std::string JSONHeuristicTypeEntry = "HeuristicType";

	/*HashTable of claimed Bounds*/
	const std::string ClaimedTableJson = "{Heuristic_Bounds}:Claimed";
	const std::string ClaimedHashTableNID2BoundData = "{Heuristic_Bounds}:Claimed_NID2BID";
	const std::string ClaimedHashTableBoundID2NID = "{Heuristic_Bounds}:Claimed_BID2NID";

	IHeuristic::Type ActiveHeuristic = IHeuristic::Type::eNone;
	std::shared_ptr<IHeuristic> CachedHeuristic;
	long long CachedHeuristicTimeStamp = 0;
	std::shared_mutex HeuristicFetchMutex;

   public:
	struct PendingBoundStruct
	{
		IBounds::BoundsID ID;
		std::string BoundsDataBase64;
		void to_json(Json& j) const
		{
			j["ID"] = ID;
			j["BoundsData64"] = BoundsDataBase64;
		}
		void from_json(const Json& j)
		{
			ID = j["ID"];
			BoundsDataBase64 = j["BoundsData64"];
		}
	};
	struct ClaimedBoundStruct
	{
		IBounds::BoundsID ID;
		NetworkIdentity identity;
		std::string BoundsDataBase64;
		void to_json(Json& j) const
		{
			j["ID"] = ID;
			ByteWriter bw;
			identity.Serialize(bw);
			j["Owner64"] = bw.as_string_base_64();
			j["OwnerName"] = identity.ToString();
			j["BoundsData64"] = BoundsDataBase64;
		}
		void from_json(const Json& j)
		{
			ID = j["ID"];
			std::string base64IdentityData = j["Owner64"].get<std::string>();
			ByteReader br(base64IdentityData, true);
			identity.Deserialize(br);
			BoundsDataBase64 = j["BoundsData64"];
		}
	};
	[[nodiscard]] IHeuristic::Type GetActiveHeuristicType() const;

	std::optional<IBounds::BoundsID> BoundIDFromShard(const NetworkIdentity& id)
	{
		ASSERT(id.Type == NetworkIdentityType::eShard, "Invalid Networking Identity");
		ByteWriter bw;
		id.Serialize(bw);

		std::string owner64 = bw.as_string_base_64();
		const auto result = InternalDB::Get()->WithSync(
			[&](auto& r) -> std::optional<IBounds::BoundsID>
			{
				std::array<std::string, 3> get_id_cmd = {
					"JSON.GET", JSONDataTable,
					std::format("$.{}.[?(@.Owner64=='{}')]", JSONClaimedEntry, owner64)};
				std::optional<std::string> response =
					r.template command<std::optional<std::string>>(get_id_cmd.begin(),
																   get_id_cmd.end());

				const Json json = response.has_value() ? Json::parse(*response) : Json();

				{
					std::string cmd;
					for (const auto& arg : get_id_cmd) cmd += arg + " ";
					// logger.DebugFormatted("The Command [{}] responded with [\"{}\"] [{}]", cmd,
					//					  json.dump(4), response.value_or("NO RESPONSE"));
				}

				if (json.is_null() || !json.is_array() || (json.size() == 0) ||
					json.front().is_null())
				{
					return std::nullopt;
				}
				ASSERT(json.is_array(), "Invalid response");
				ASSERT(json.size() <= 1, "This should never occur");
				ASSERT(json.front().contains("ID"), "Invalid entry");
				return json.front()["ID"].get<IBounds::BoundsID>();
			});
		if (!result.has_value())
		{
			logger.WarningFormatted("BoundIDFromShard returned nothing");
		}
		else
		{
			// logger.DebugFormatted("BoundIDFromShard returned {}", *result);
		}
		return result;
	}
	/* template <typename BoundType, typename KeyType = std::string> */
	/* void GetAllPendingBounds(std::vector<BoundType>& out_bounds); */

	/* template <typename BoundType, typename KeyType = std::string> */
	/* void StorePendingBounds(const std::vector<BoundType>& in_bounds); */

	[[nodiscard]] std::unique_ptr<IBounds> ClaimNextPendingBound(const NetworkIdentity& claim_key);

	void StorePendingBoundsFromByteWriters(
		const std::unordered_map<IBounds::BoundsID, ByteWriter>& in_writers);

	void GetPendingBoundsAsByteReaders(std::vector<std::string>& data_for_readers,
									   std::unordered_map<IBounds::BoundsID, ByteReader>& brs);

	void GetClaimedBoundsAsByteReaders(
		std::vector<std::string>& data_for_readers,
		std::unordered_map<NetworkIdentity, std::pair<IBounds::BoundsID, ByteReader>>& brs);

	std::optional<ClaimedBoundStruct> GetClaimedBound(IBounds::BoundsID);
	[[nodiscard]] long long GetPendingBoundsCount() const;

	[[nodiscard]] long long GetClaimedBoundsCount() const;

	bool RequeueClaimedBound(const std::string_view& claim_key);

	template <typename BoundType>
	void GetAllClaimedBounds(std::unordered_map<NetworkIdentity, BoundType>& out_bounds);

	template <typename FN>
		requires std::is_invocable_v<FN, const IHeuristic&>
	auto PullHeuristic(FN&& f)
	{
		const std::optional<std::tuple<long long, IHeuristic::Type, std::string>> FetchResult =
			InternalDB::Get()->WithSync(
				[&](auto& r) -> std::optional<std::tuple<long long, IHeuristic::Type, std::string>>
				{
					const std::string FetchScript = R"(
-- KEYS[1] = Redis JSON key
-- ARGV[1] = timestamp path (e.g. $.timestamp)
-- ARGV[2] = path for HeuristicType
-- ARGV[3] = path for HeuristicData64
-- ARGV[4] = client timestamp

local client_ts = tonumber(ARGV[4])

-- get stored timestamp
local stored = redis.call("JSON.GET", KEYS[1], ARGV[1])
if not stored then
    return nil
end

local stored_ts = tonumber(stored)

-- if client timestamp is older, return timestamp and both entries
if client_ts < stored_ts then
    local type_val = redis.call("JSON.GET", KEYS[1], ARGV[2])
    local data_val = redis.call("JSON.GET", KEYS[1], ARGV[3])
    return {tostring(stored_ts), type_val, data_val}
end

return nil
        )";

					const std::optional<std::vector<std::string>> result =
						r.template eval<std::optional<std::vector<std::string>>>(
							FetchScript, {JSONDataTable},
							{HeuristicTimestampKey, JSONHeuristicTypeEntry,
							 JSONHeuristicData64Entry, std::to_string(CachedHeuristicTimeStamp)});

					if (result.has_value() && result->size() == 3)
					{
						logger.Debug("Cached Heuristic out of date. Fetching new...");
						std::string_view encodedTypeEnum = (*result)[1];
						if (encodedTypeEnum.size() >= 2 && encodedTypeEnum.front() == '"' && encodedTypeEnum.back() == '"')
						{
							encodedTypeEnum = encodedTypeEnum.substr(1, encodedTypeEnum.size() - 2);
						}
						//const std::string s =
						//	result.has_value()
						//		? ((*result)[0] + " " + (*result)[1] + " " + (*result)[2] + " ")
						//		: "NOTHING";
						//logger.DebugFormatted("Pull Heuristic returned {}", s);
						IHeuristic::Type t;
						IHeuristic::TypeFromString(encodedTypeEnum, t);
						return std::make_tuple(std::stoull((*result)[0]),  // timestamp
											   t,						   // HeuristicType
											   (*result)[2]				   // HeuristicData64
						);
					}
					return std::nullopt;
				});

		if (FetchResult.has_value())
		{
			std::unique_lock lock(HeuristicFetchMutex);
			CachedHeuristicTimeStamp = std::get<0>(*FetchResult);
			Internal_ParseHeuristic64(std::get<1>(*FetchResult), std::get<2>(*FetchResult));
		}

		std::shared_lock lock(HeuristicFetchMutex);

		return f(*CachedHeuristic);
	}
	void PushHeuristic(const IHeuristic& h);
	[[nodiscard]] std::optional<NetworkIdentity> ShardFromPosition(const Transform& t);

	[[nodiscard]] std::optional<NetworkIdentity> ShardFromBoundID(const IBounds::BoundsID id);

   private:
	void Internal_SetActiveHeuristicType(IHeuristic::Type type);
	void Internal_InsertPendingBound(const PendingBoundStruct& p);
	void Internal_InsertClaimedBound(const ClaimedBoundStruct& p);
	void Internal_EnsureJsonTable();
	void Internal_ParseHeuristic64(IHeuristic::Type type, std::string_view data64);
	std::unique_ptr<IBounds> Internal_CreateIBoundInst();
	std::optional<ClaimedBoundStruct> Internal_PullClaimedBound(IBounds::BoundsID id);
};
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