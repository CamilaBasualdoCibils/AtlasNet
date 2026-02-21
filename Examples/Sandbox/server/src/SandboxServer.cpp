#include "SandboxServer.hpp"

#include <chrono>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Global/Misc/UUID.hpp"

void SandboxServer::Run()
{
	AtlasNetServer::InitializeProperties InitProperties;
	InitProperties.OnShutdownRequest = [&](SignalType signal) { ShouldShutdown = true; };
	AtlasNetServer::Get().Initialize(InitProperties);

	EventSystem::Get().Subscribe<ClientConnectEvent>(
		[&](const ClientConnectEvent& e)
		{
			logger.DebugFormatted("Client Connected event!\n - ID: {}\n - IP: {}\n - Proxy: {}",
								  UUIDGen::ToString(e.client.ID), e.client.ip.ToString(),
								  e.ConnectedProxy.ToString());
		});

	Transform t;
	t.position = {0, 0, 0};
	AtlasEntityHandle e = AtlasNetServer::Get().CreateEntity(t);
		
	while (!ShouldShutdown)
	{
		const auto LocalEntities = AtlasNetServer::Get().ViewLocalEntities();

		//logger.DebugFormatted("Processing {} entities", LocalEntities.size());
		for (AtlasEntity& e : LocalEntities)
		{
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}
