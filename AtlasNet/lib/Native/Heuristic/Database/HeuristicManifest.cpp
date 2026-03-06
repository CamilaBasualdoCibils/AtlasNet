#include "HeuristicManifest.hpp"

#include <sw/redis++/redis.h>

#include <boost/container/small_vector.hpp>
#include <boost/describe/enum_from_string.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Quadtree/QuadtreeHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"
#include "Heuristic/Voronoi/VoronoiHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
std::optional<Json> ParseRedisJsonPayload(const std::optional<std::string>& payload)
{
	if (!payload || payload->empty() || *payload == "null")
	{
		return std::nullopt;
	}

	Json parsed = Json::parse(*payload, nullptr, false);
	if (parsed.is_discarded())
	{
		return std::nullopt;
	}
	if (parsed.is_array())
	{
		if (parsed.empty())
		{
			return std::nullopt;
		}
		parsed = parsed.front();
	}
	return parsed;
}
}  // namespace
static std::string_view StripQuotes(std::string_view str)
{
	if (!str.empty())
	{
		// Remove leading character if it is '[' or '"'
		if (str.front() == '"')
			str.remove_prefix(1);
		// Remove trailing character if it is ']' or '"'
		if (!str.empty() && str.back() == '"')
			str.remove_suffix(1);
	}
	return str;	 // make a new string to own the memory
}

long long HeuristicManifest::GetPendingBoundsCount() const
{
	return InternalDB::Get()->SCard(PendingIDsSet);
}
long long HeuristicManifest::GetClaimedBoundsCount() const
{
	return InternalDB::Get()->HLen(NID2BoundIDMap);
}

bool HeuristicManifest::ReleaseClaimedBound(IBounds::BoundsID ID)
{
	const std::optional<NetworkIdentity> NetID = ShardFromBoundID(ID);
	if (!NetID.has_value())
		return false;
	ByteWriter bw;
	NetID->Serialize(bw);

	const bool claimed = InternalDB::Get()->HExists(BoundID2NIDMap, std::to_string(ID));
	if (claimed)
	{
		InternalDB::Get()->HDel(BoundID2NIDMap, {std::to_string(ID)});
		InternalDB::Get()->HDel(NID2BoundIDMap, {bw.as_string_view()});
		return true;
	}
	return false;
}
void HeuristicManifest::Internal_SetActiveHeuristicType(IHeuristic::Type type)
{
	logger.DebugFormatted("Set HeuristicType {}", IHeuristic::TypeToString(type));
	InternalDB::Get()->Set(HeuristicTypeKey, IHeuristic::TypeToString(type));
}
IHeuristic::Type HeuristicManifest::GetActiveHeuristicType() const
{
	const auto stripped_string =
		StripQuotes(InternalDB::Get()->Get(HeuristicTypeKey).value_or("INVALID"));

	IHeuristic::Type t;
	IHeuristic::TypeFromString(stripped_string, t);
	return t;
}

std::optional<NetworkIdentity> HeuristicManifest::ShardFromPosition(const Transform& t)
{
	const auto boundID =
		PullHeuristic([&](const IHeuristic& h) { return h.QueryPosition(t.position); });
	if (!boundID.has_value())
	{
		return std::nullopt;
	}
	//logger.DebugFormatted("found bound for position {}", *boundID);
	return ShardFromBoundID(*boundID);
	// bound->GetID()
}
std::optional<NetworkIdentity> HeuristicManifest::ShardFromBoundID(const IBounds::BoundsID id)
{
	const std::optional<std::string> shard =
		InternalDB::Get()->HGet(BoundID2NIDMap, std::to_string(id));

	if (shard.has_value())
	{
		NetworkIdentity shardID;
		ByteReader br(shard.value());
		shardID.Deserialize(br);
		return shardID;
	}
	return std::nullopt;
}

void HeuristicManifest::PushHeuristic(const IHeuristic& h)
{
	std::unique_lock lock(HeuristicFetchMutex);
	Internal_SetActiveHeuristicType(h.GetType());
	ByteWriter bw;
	h.Serialize(bw);
	InternalDB::Get()->Set(HeuristicSerializeDataKey, bw.as_string_view());

	logger.DebugFormatted("Pushed Heuristic type {} with {} bounds",
						  IHeuristic::TypeToString(h.GetType()), h.GetBoundsCount());
	const size_t previousBoundCount =
		InternalDB::Get()->SCard(PendingIDsSet) + InternalDB::Get()->HLen(BoundID2NIDMap);
	boost::container::small_vector<std::string, 16> newEntries;
	newEntries.reserve(h.GetBoundsCount());
	std::vector<std::string_view> newEntriesViews;
	for (size_t i = previousBoundCount; i < h.GetBoundsCount(); i++)
	{
		newEntries.push_back(std::to_string(i));
		newEntriesViews.push_back(newEntries.back());
	}
	InternalDB::Get()->SAdd(PendingIDsSet, newEntriesViews);
	const auto t = InternalDB::Get()->GetTimeNow();

	const long long ms = t.seconds * 1000 + t.microseconds / 1000;
	InternalDB::Get()->Set(HeuristicTimestampKey, std::to_string(ms));
	logger.DebugFormatted("Pushed {} new bounds", newEntriesViews.size());
	logger.Debug("Heuristic Pushed");
}

std::optional<IBounds::BoundsID> HeuristicManifest::ClaimNextPendingBound(
	const NetworkIdentity& claim_key)
{
	const std::optional<std::string> ID_s = InternalDB::Get()->SPop(PendingIDsSet);
	if (!ID_s.has_value())
	{
		return std::nullopt;
	}

	IBounds::BoundsID ID = std::stoi(*ID_s);

	ByteWriter bw;
	claim_key.Serialize(bw);
	InternalDB::Get()->HSet(NID2BoundIDMap, bw.as_string_view(), ID_s.value());
	InternalDB::Get()->HSet(BoundID2NIDMap, ID_s.value(), bw.as_string_view());
	return ID;
}

void HeuristicManifest::Internal_ParseHeuristic(IHeuristic::Type type, const std::string_view data)
{
	switch (type)
	{
		case IHeuristic::Type::eGridCell:
			CachedHeuristic = std::make_unique<GridHeuristic>();
			break;
		case IHeuristic::Type::eQuadtree:
			CachedHeuristic = std::make_unique<QuadtreeHeuristic>();
			break;
		case IHeuristic::Type::eVoronoi:
			CachedHeuristic = std::make_unique<VoronoiHeuristic>();
			break;

		default:
		case IHeuristic::Type::eOctree:
		case IHeuristic::Type::eNone:
			ASSERT(false, "INVALID HEURISTIC TYPE");
			throw "ERROR";
			break;
	}
	try
	{
		ByteReader br(data);
		CachedHeuristic->Deserialize(br);
	}
	catch (...)
	{
		logger.Warning("Failed to deserialize heuristic payload due to unknown error.");
		throw "FAILURE";
	}
}
std::optional<IBounds::BoundsID> HeuristicManifest::BoundIDFromShard(const NetworkIdentity& id)
{
	ASSERT(id.Type == NetworkIdentityType::eShard, "Invalid Networking Identity");
	ByteWriter bw;
	id.Serialize(bw);

	const std::optional<std::string> BoundID_S =
		InternalDB::Get()->HGet(NID2BoundIDMap, bw.as_string_view());
	if (BoundID_S.has_value())
	{
		return std::stoi(BoundID_S.value());
	}
	return std::nullopt;
}
