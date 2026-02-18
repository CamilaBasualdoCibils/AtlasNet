#include "EntityHandoff/HandoffConnectionManager.hpp"

#include "Database/ServerRegistry.hpp"
#include "EntityHandoff/HandoffPacketManager.hpp"
#include "Interlink.hpp"

namespace
{
constexpr auto kConnectionFlapInterval = std::chrono::seconds(5);
}

void HandoffConnectionManager::SelectTestTargetShard()
{
	const auto& servers = ServerRegistry::Get().GetServers();
	for (const auto& [id, _entry] : servers)
	{
		if (id.Type != NetworkIdentityType::eShard)
		{
			continue;
		}
		if (id == selfIdentity)
		{
			continue;
		}
		testTargetIdentity = id;
		if (logger)
		{
			logger->DebugFormatted(
				"[EntityHandoff] Connection flap test target selected shard: {}",
				testTargetIdentity->ToString());
		}
		return;
	}

	testTargetIdentity.reset();
	if (logger)
	{
		logger->Warning(
			"[EntityHandoff] No peer shard found in ServerRegistry for flap test");
	}
}

void HandoffConnectionManager::Init(const NetworkIdentity& self,
									std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	testConnectionActive = false;
	lastToggleTime = std::chrono::steady_clock::now() - kConnectionFlapInterval;
	HandoffPacketManager::Get().Init(selfIdentity, logger);
	SelectTestTargetShard();

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffConnectionManager initialized for {}",
			selfIdentity.ToString());
	}
}

void HandoffConnectionManager::Tick()
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - lastToggleTime < kConnectionFlapInterval)
	{
		return;
	}

	lastToggleTime = now;
	if (!testTargetIdentity.has_value())
	{
		SelectTestTargetShard();
		return;
	}

	if (!testConnectionActive)
	{
		Interlink::Get().EstablishConnectionTo(*testTargetIdentity);
		HandoffPacketManager::Get().SendPing(*testTargetIdentity);
		testConnectionActive = true;
		if (logger)
		{
			logger->DebugFormatted(
				"[EntityHandoff] Flap test: connect -> {}",
				testTargetIdentity->ToString());
		}
		return;
	}

	Interlink::Get().CloseConnectionTo(*testTargetIdentity, 0,
									   "EntityHandoff flap test");
	testConnectionActive = false;
	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] Flap test: disconnect -> {}",
			testTargetIdentity->ToString());
	}
}

void HandoffConnectionManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	if (testTargetIdentity.has_value() && testConnectionActive)
	{
		Interlink::Get().CloseConnectionTo(*testTargetIdentity, 0,
										   "EntityHandoff shutdown");
	}

	testConnectionActive = false;
	testTargetIdentity.reset();
	HandoffPacketManager::Get().Shutdown();
	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] HandoffConnectionManager shutdown");
	}
}
