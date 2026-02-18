#pragma once

#include <memory>

#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

class HandoffPingPacket;

class HandoffPacketManager : public Singleton<HandoffPacketManager>
{
  public:
	HandoffPacketManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Shutdown();
	void SendPing(const NetworkIdentity& target) const;

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void OnHandoffPingPacket(const HandoffPingPacket& packet) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	PacketManager::Subscription handoffPingSub;
};
