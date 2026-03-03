#include "Partition.hpp"

#include <chrono>
#include <thread>

#include "Entity/EntityLedger.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Interlink/Database/HealthManifest.hpp"
// #include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityManager.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
Partition::Partition() {}
Partition::~Partition() {}

namespace
{
constexpr std::chrono::milliseconds kPartitionTickInterval(32);
}  // namespace

void Partition::Init()
{
	NetworkIdentity partitionIdentifier(NetworkIdentityType::eShard, UUIDGen::Gen());
	NetworkCredentials::Make(partitionIdentifier);

	logger = std::make_shared<Log>(partitionIdentifier.ToString());

	HealthManifest::Get().ScheduleHealthPings();
	NetworkManifest::Get().ScheduleNetworkPings();
	Interlink::Get().Init();
	BoundLeaser::Get().Init();
	EntityLedger::Get().Init();

	while (!ShouldShutdown)
	{
		std::this_thread::sleep_for(kPartitionTickInterval);  // ~30 updates/sec
		// NH_EntityAuthorityManager::Get().Tick();
	}

	logger->Debug("Shutting Down");
	// NH_EntityAuthorityManager::Get().Shutdown();
	Interlink::Get().Shutdown();
}
