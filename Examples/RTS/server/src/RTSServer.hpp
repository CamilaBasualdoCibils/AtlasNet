#pragma once

#include <atomic>

#include "AtlasNetServer.hpp"
#include "Command/NetCommand.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "commands/PlayerCameraMoveCommand.hpp"
class RTSServer : public Singleton<RTSServer>, public IAtlasNetServer
{
	Log logger = Log("RTSServer");
	std::atomic_bool ShouldShutdown = false;

   public:
	RTSServer();
	void Run();
	void Shutdown() override;

   private:
	void OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
					   AtlasEntityPayload& payload) override;

	void OnClientCameraMoveCommand(const NetClientIntentHeader& header,
								   const PlayerCameraMoveCommand& c);

	void SendGameStateData();
};
