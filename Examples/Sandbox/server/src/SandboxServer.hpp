#pragma once

#include <atomic>

#include <SandboxWorld.hpp>

#include "AtlasNetServer.hpp"
#include "Command/NetCommand.hpp"
#include "Commands/GameClientInputCommand.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"

class SandboxServer : IAtlasNetServer
{
	Log logger = Log("Sandbox");
	SandboxWorld world;
	std::atomic_bool ShouldShutdown = false;

   public:
	void Run();
	void Shutdown() override;

	private:
	 void OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
							   AtlasEntityPayload& payload) override;
	 void OnGameClientInputCommand(const NetClientIntentHeader& header,
								   const GameClientInputCommand& command);
};
