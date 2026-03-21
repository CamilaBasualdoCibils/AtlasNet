#include "HeuristicManifest.hpp"

#include <sw/redis++/redis.h>

#include <boost/container/small_vector.hpp>
#include <boost/describe/enum_from_string.hpp>
#include <charconv>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Quadtree/QuadtreeHeuristic.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/LlmVoronoiHeuristic.hpp"
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

bool HeuristicManifest::ReleaseClaimedBound(const NetworkIdentity& NetID)
{
	const std::string NetIDStr = UUIDGen::ToString(NetID.ID);

	const bool claimed = InternalDB::Get()->HExists(NID2BoundIDMap, NetIDStr);
	if (claimed)
	{
		const std::optional<std::string> boundID_s =
			InternalDB::Get()->HGet(NID2BoundIDMap, NetIDStr);
		ASSERT(boundID_s.has_value(), "INVALID SCENARIO");

		BoundsID boundID = 0;
		const auto [ptr, ec] =
			std::from_chars(boundID_s->data(), boundID_s->data() + boundID_s->size(), boundID);

		(void)InternalDB::Get()->HDel(NID2BoundIDMap, {NetIDStr});
		if (ec == std::errc() && Internal_IsValidBoundID(boundID))
		{
			(void)InternalDB::Get()->SAdd(PendingIDsSet, {*boundID_s});
		}
		else
		{
			logger.WarningFormatted(
				"Dropping stale claimed bound {} for {} during release; it no longer exists in the active heuristic.",
				boundID_s.value(), NetID.ToString());
		}
		Internal_OwnershipTableIncreaseVersion();
		return true;
	}
	return false;
}
void HeuristicManifest::Internal_SetActiveHeuristicType(IHeuristic::Type type)
{
	logger.DebugFormatted("Set HeuristicType {}", IHeuristic::TypeToString(type));
	InternalDB::Get()->Set(HeuristicTypeKey, IHeuristic::TypeToString(type));
}

void HeuristicManifest::PushHeuristic(const IHeuristic& h)
{
	std::unique_lock lock(HeuristicFetchMutex);
	Internal_SetActiveHeuristicType(h.GetType());
	ByteWriter bw;
	h.Serialize(bw);
	InternalDB::Get()->Set(HeuristicSerializeDataKey, bw.as_string_view());
	Internal_ReconcileOwnershipState(h.GetBoundsCount());

	logger.DebugFormatted("Pushed Heuristic type {} with {} bounds",
						  IHeuristic::TypeToString(h.GetType()), h.GetBoundsCount());
	const auto t = InternalDB::Get()->GetTimeNow();

	if (InternalDB::Get()->Exists(HeuristicVersionKey))
	{
		InternalDB::Get()->WithSync([&](auto& r) { return r.incr(HeuristicVersionKey); });
	}
	else
	{
		InternalDB::Get()->Set(HeuristicVersionKey, std::to_string(1));
	}
	logger.DebugFormatted("Reconciled ownership for {} bounds", h.GetBoundsCount());
	logger.Debug("Heuristic Pushed");
}

std::optional<BoundsID> HeuristicManifest::ClaimNextPendingBound(const NetworkIdentity& claim_key)
{
	const std::string Claim_Key_s = UUIDGen::ToString(claim_key.ID);
	while (true)
	{
		const std::optional<std::string> ID_s = InternalDB::Get()->SPop(PendingIDsSet);
		if (!ID_s.has_value())
		{
			return std::nullopt;
		}

		BoundsID BID = 0;
		const auto [ptr, ec] =
			std::from_chars(ID_s->data(), ID_s->data() + ID_s->size(), BID);
		const bool parsed = (ec == std::errc()) && (ptr == ID_s->data() + ID_s->size());
		if (!parsed)
		{
			logger.WarningFormatted("Dropping malformed pending bound id '{}'", ID_s.value());
			continue;
		}
		if (!Internal_IsValidBoundID(BID))
		{
			logger.WarningFormatted(
				"Dropping stale pending bound {} while claiming for {}; it no longer exists in the active heuristic.",
				BID, claim_key.ToString());
			continue;
		}

		InternalDB::Get()->HSet(NID2BoundIDMap, Claim_Key_s, ID_s.value());
		Internal_OwnershipTableIncreaseVersion();
		return BID;
	}
}

bool HeuristicManifest::Internal_IsValidBoundID(BoundsID boundID)
{
	try
	{
		return PullHeuristic(
			[&](const IHeuristic& h)
			{
				return static_cast<uint64_t>(boundID) <
					   static_cast<uint64_t>(h.GetBoundsCount());
			});
	}
	catch (const std::exception& ex)
	{
		logger.WarningFormatted("Failed to validate bound {} against active heuristic: {}",
								boundID, ex.what());
	}
	catch (...)
	{
		logger.WarningFormatted("Failed to validate bound {} against active heuristic.",
								boundID);
	}
	return false;
}

void HeuristicManifest::Internal_ReconcileOwnershipState(uint32_t boundsCount)
{
	std::unique_lock ownershipLock(OwnershitFetchMutex);

	const auto claimedEntries = InternalDB::Get()->HGetAll(NID2BoundIDMap);
	std::unordered_set<BoundsID> claimedBoundIDs;
	claimedBoundIDs.reserve(claimedEntries.size());

	std::vector<std::string> shardClaimsToDrop;
	shardClaimsToDrop.reserve(claimedEntries.size());

	for (const auto& [shardID, boundIDText] : claimedEntries)
	{
		BoundsID boundID = 0;
		const auto [ptr, ec] = std::from_chars(boundIDText.data(),
											   boundIDText.data() + boundIDText.size(), boundID);
		const bool parsed = (ec == std::errc()) && (ptr == boundIDText.data() + boundIDText.size());
		const bool inRange =
			parsed && static_cast<uint64_t>(boundID) < static_cast<uint64_t>(boundsCount);
		if (!inRange || !claimedBoundIDs.insert(boundID).second)
		{
			shardClaimsToDrop.push_back(shardID);
		}
	}

	if (!shardClaimsToDrop.empty())
	{
		std::vector<std::string_view> claimViews;
		claimViews.reserve(shardClaimsToDrop.size());
		for (const auto& shardID : shardClaimsToDrop)
		{
			claimViews.push_back(shardID);
		}
		(void)InternalDB::Get()->HDel(NID2BoundIDMap, claimViews);
	}

	(void)InternalDB::Get()->DelKey(PendingIDsSet);

	std::vector<std::string> pendingBoundIDs;
	pendingBoundIDs.reserve(boundsCount);
	for (BoundsID boundID = 0; static_cast<uint64_t>(boundID) < static_cast<uint64_t>(boundsCount);
		 ++boundID)
	{
		if (claimedBoundIDs.contains(boundID))
		{
			continue;
		}
		pendingBoundIDs.push_back(std::to_string(boundID));
	}

	if (!pendingBoundIDs.empty())
	{
		std::vector<std::string_view> pendingViews;
		pendingViews.reserve(pendingBoundIDs.size());
		for (const auto& pendingID : pendingBoundIDs)
		{
			pendingViews.push_back(pendingID);
		}
		(void)InternalDB::Get()->SAdd(PendingIDsSet, pendingViews);
	}

	Internal_OwnershipTableIncreaseVersion();
	logger.DebugFormatted(
		"Reconciled ownership state: bounds={}, kept_claims={}, dropped_claims={}, pending={}",
		boundsCount, claimedBoundIDs.size(), shardClaimsToDrop.size(), pendingBoundIDs.size());
}

void HeuristicManifest::Internal_ParseHeuristic(IHeuristic::Type type, const std::string_view data)
{
	switch (type)
	{
		case IHeuristic::Type::eGridCell:
			localHeuristicCache.CachedHeuristic = std::make_unique<GridHeuristic>();
			break;
		case IHeuristic::Type::eQuadtree:
			localHeuristicCache.CachedHeuristic = std::make_unique<QuadtreeHeuristic>();
			break;
		case IHeuristic::Type::eVoronoi:
			localHeuristicCache.CachedHeuristic = std::make_unique<VoronoiHeuristic>();
			break;
		case IHeuristic::Type::eHotspotVoronoi:
			localHeuristicCache.CachedHeuristic =
				std::make_unique<HotspotVoronoiHeuristic>();
			break;
		case IHeuristic::Type::eLlmVoronoi:
			localHeuristicCache.CachedHeuristic =
				std::make_unique<LlmVoronoiHeuristic>();
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
		localHeuristicCache.CachedHeuristic->Deserialize(br);
	}
	catch (...)
	{
		logger.Warning("Failed to deserialize heuristic payload due to unknown error.");
		throw "FAILURE";
	}
}
