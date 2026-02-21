#include "BoundLeaser.hpp"

#include "Heuristic/Database/HeuristicManifest.hpp"

void BoundLeaser::ClaimBound()
{
	logger.Debug("Claiming the next pending bound");
	ClaimedBound = HeuristicManifest::Get().ClaimNextPendingBound(ID);
	ASSERT(ClaimedBound, "ClaimNextPendingBound returned null");
	ClaimedBoundID = ClaimedBound->GetID();
	logger.DebugFormatted("Claiming bound id {}", ClaimedBoundID);

	ASSERT(ClaimedBoundID == HeuristicManifest::Get().BoundIDFromShard(ID).value(), "Internal Error");
}
