#include "Partition.hpp"

#include <chrono>
#include <thread>

#include "Database/HealthManifest.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink.hpp"
#include "Misc/UUID.hpp"
#include "Packet/CommandPacket.hpp"
#include "Telemetry/NetworkManifest.hpp"
#include "pch.hpp"
Partition::Partition() {}
Partition::~Partition() {}

void Partition::Init()
{
	NetworkIdentity partitionIdentifier(NetworkIdentityType::eShard,
										UUIDGen::Gen());

	logger = std::make_shared<Log>(partitionIdentifier.ToString());

	HealthManifest::Get().ScheduleHealthPings(partitionIdentifier);
	NetworkManifest::Get().ScheduleNetworkPings(partitionIdentifier);
	Interlink::Get().Init(
		InterlinkProperties{.ThisID = partitionIdentifier, .logger = logger});

	{
		GridShape claimedBounds;
		const bool claimed =
			HeuristicManifest::Get().ClaimNextPendingBound<GridShape>(
				partitionIdentifier.ToString(), claimedBounds);
		if (claimed)
		{
			logger->DebugFormatted("Claimed bounds {} for shard",
								   claimedBounds.GetID());
		}
		else
		{
			logger->Warning("No pending bounds available to claim");
		}
	}
	// Clear any existing partition entity data to prevent stale data
	while (!ShouldShutdown)
	{
		std::this_thread::sleep_for(
			std::chrono::milliseconds(32));	 // ~30 updates per second
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}
