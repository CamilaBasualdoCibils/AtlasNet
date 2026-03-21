#include "BoundLeaser.hpp"

#include <string>

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Snapshot/SnapshotService.hpp"

void BoundLeaser::ClaimBound()
{
	const auto& SelfID = NetworkCredentials::Get().GetID();
	logger.Debug("Claiming the next pending bound");
	const std::optional<BoundsID> claimedBoundID =
		HeuristicManifest::Get().ClaimNextPendingBound(SelfID);
	{
		std::lock_guard lock(ClaimedBoundMutex);
		ClaimedBoundID = claimedBoundID;
	}
	if (!claimedBoundID.has_value())
	{
		logger.Warning("No pending bounds available yet; retrying.");
		return;
	}
	logger.DebugFormatted("Claiming bound id {}", claimedBoundID.value());
	SnapshotService::Get().RecoverClaimedBoundSnapshotIfNeeded();
	/*
	const auto boundIdInManifest = HeuristicManifest::Get().BoundIDFromShard(SelfID);
	if (!boundIdInManifest.has_value() || *boundIdInManifest != ClaimedBoundID)
	{
		logger.WarningFormatted(
			"Claimed bound {} but manifest reports {} for self shard.",
			ClaimedBoundID.value(), boundIdInManifest.has_value() ? std::to_string(*boundIdInManifest)
														 : std::string("none"));
		}*/
}

void BoundLeaser::ClearInvalidClaimedBound(BoundsID claimedBoundID)
{
	ClearClaimedBound();
	const auto& selfID = NetworkCredentials::Get().GetID();
	if (!HeuristicManifest::Get().ReleaseClaimedBound(selfID))
	{
		logger.WarningFormatted(
			"Cleared stale local bound {} for {}, but no matching manifest claim was present.",
			claimedBoundID, selfID.ToString());
	}
}
