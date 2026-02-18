#include "EntityHandoff/HandoffPacketManager.hpp"

#include <chrono>

#include "EntityHandoff/HandoffConnectionManager.hpp"
#include "EntityHandoff/Packet/HandoffPingPacket.hpp"
#include "Interlink.hpp"

void HandoffPacketManager::Init(const NetworkIdentity& self,
								std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	handoffPingSub =
		Interlink::Get().GetPacketManager().Subscribe<HandoffPingPacket>(
			[this](const HandoffPingPacket& packet)
			{ OnHandoffPingPacket(packet); });

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffPacketManager initialized for {}",
			selfIdentity.ToString());
		logger->Debug("[EntityHandoff] Subscribed to HandoffPingPacket");
	}
}

void HandoffPacketManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	handoffPingSub.Reset();
	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] HandoffPacketManager shutdown");
	}
}

void HandoffPacketManager::SendPing(const NetworkIdentity& target) const
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
						 std::chrono::system_clock::now())
						 .time_since_epoch()
						 .count();

	HandoffPingPacket packet;
	packet.sender = selfIdentity;
	packet.sentAtMs = static_cast<uint64_t>(now);
	Interlink::Get().SendMessage(target, packet,
								 NetworkMessageSendFlag::eReliableNow);

	if (logger)
	{
		logger->DebugFormatted("[EntityHandoff] Sent HandoffPingPacket to {}",
							   target.ToString());
	}
}

void HandoffPacketManager::OnHandoffPingPacket(
	const HandoffPingPacket& packet) const
{
	if (!initialized || !logger)
	{
		return;
	}

	const auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(
						   std::chrono::system_clock::now())
						   .time_since_epoch()
						   .count();
	const uint64_t receiveMs = static_cast<uint64_t>(nowMs);
	const uint64_t latencyMs =
		receiveMs >= packet.sentAtMs ? (receiveMs - packet.sentAtMs) : 0;

	logger->DebugFormatted(
		"[EntityHandoff] Received HandoffPingPacket from {} latency={}ms "
		"sentAt={} recvAt={}",
		packet.sender.ToString(), latencyMs, packet.sentAtMs, receiveMs);

	HandoffConnectionManager::Get().MarkConnectionActivity(packet.sender);
}
