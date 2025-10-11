#include "AtlasNetServer.hpp"

#include "Interlink/Interlink.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties &properties)
{
	CrashHandler::Get().Init(properties.ExePath);
	DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});
	logger->Print("AtlasNet Initialize");
	InterLinkIdentifier myID = InterLinkIdentifier(InterlinkType::eGameServer, DockerIO::Get().GetSelfContainerName());
	IPAddress addrs;
	addrs.Parse(DockerIO::Get().GetSelfContainerIP() + ":" + std::to_string(_PORT_GAMESERVER));
	ServerRegistry::Get().RegisterSelf(myID, addrs);
	Interlink::Check();
	Interlink::Get().Init(
		InterlinkProperties{.ThisID = myID,
							.logger = logger,
							.callbacks = {.acceptConnectionCallback = [](const Connection &c)
										  { return true; },
										  .OnConnectedCallback = nullptr}});
}

void AtlasNetServer::Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
							std::vector<AtlasEntityID> &OutgoingEntities)
{
	Interlink::Get().Tick();
}
