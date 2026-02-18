#include "AtlasNetServer.hpp"


#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
#include "Global/Misc/UUID.hpp"
// ============================================================================
// Initialize server and setup Interlink callbacks
// ============================================================================
void AtlasNetServer::Initialize(
	AtlasNetServer::InitializeProperties &properties)
{
	// --- Core setup ---
	// CrashHandler::Get().Init();
	CrashHandler::Get().Init();
	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});

	NetworkIdentity myID(NetworkIdentityType::eShard, UUIDGen::Gen());
	logger = std::make_shared<Log>("Shard");
	logger->Debug("AtlasNet Initialize");
	// --- Interlink setup ---
	Interlink::Get().Init({
		.ThisID = myID,
		.logger = logger,
	});

	logger->Debug("Interlink initialized");
}

// ============================================================================
// Update: Called every tick
// Sends entity updates, incoming, and outgoing data to Partition
// ============================================================================
void AtlasNetServer::Update(
	std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
	std::vector<AtlasEntityID> &OutgoingEntities)
{
}
