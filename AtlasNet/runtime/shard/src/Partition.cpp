#include "Partition.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "Packet/CommandPacket.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
#include "Database/HealthManifest.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink.hpp"
#include "Misc/UUID.hpp"
#include "Packet/CommandPacket.hpp"
#include "Telemetry/NetworkManifest.hpp"
#include "pch.hpp"
#include "EntityAtBoundsManager.hpp"
#include "EntityHandoff.hpp"
Partition::Partition()
{

}
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
		Interlink::Get().Tick();

		//std::this_thread::sleep_for(std::chrono::milliseconds(32));    // ~30 updates per second
		const float dtSeconds = 0.1f;  // update interval for test entity
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Update the single circular test entity and serialize it into bw.
		//EntityAtBoundsManager::Get().UpdateAndSerializeCircularTestEntity(bw, dtSeconds);

		//out_of_bounds_entities = EntityAtBoundsManager::Get().DetectEntitiesOutOfBounds(bw);
		//EntityHandoff::Get().InitiateHandoff(out_of_bounds_entities);
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}
