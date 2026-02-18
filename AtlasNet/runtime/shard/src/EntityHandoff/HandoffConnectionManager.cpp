#include "EntityHandoff/HandoffConnectionManager.hpp"

#include "Database/ServerRegistry.hpp"
#include "EntityHandoff/HandoffPacketManager.hpp"
#include "Interlink.hpp"

namespace
{
constexpr auto kProbeInterval = std::chrono::seconds(5);
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
				"[EntityHandoff] Selected shard target: {}",
				testTargetIdentity->ToString());
		}
		return;
	}

	testTargetIdentity.reset();
	if (logger)
	{
		logger->Warning(
			"[EntityHandoff] No peer shard found in ServerRegistry");
	}
}

void HandoffConnectionManager::Init(const NetworkIdentity& self,
									std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	testConnectionActive = false;
	lastProbeTime = std::chrono::steady_clock::now() - kProbeInterval;
	leaseCoordinator = std::make_unique<HandoffConnectionLeaseCoordinator>(
		selfIdentity, logger,
		HandoffConnectionLeaseCoordinator::Options{
			.leaseEnabled = leaseModeEnabled});

	HandoffPacketManager::Get().Init(selfIdentity, logger);
	SelectTestTargetShard();

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffConnectionManager initialized for {}",
			selfIdentity.ToString());
		const auto& options = leaseCoordinator->GetOptions();
		logger->DebugFormatted(
			"[EntityHandoff] Lease mode={} probe={}s inactivity_timeout={}s",
			leaseModeEnabled ? "enabled" : "disabled",
			std::chrono::duration_cast<std::chrono::seconds>(kProbeInterval).count(),
			std::chrono::duration_cast<std::chrono::seconds>(
				options.inactivityTimeout)
				.count());
	}
}

void HandoffConnectionManager::Tick()
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (leaseCoordinator)
	{
		leaseCoordinator->ReapInactiveConnections(
			now,
			[this](const NetworkIdentity& peer, std::chrono::seconds idleFor)
			{
				Interlink::Get().CloseConnectionTo(
					peer, 0, "EntityHandoff inactivity timeout");
				if (testTargetIdentity.has_value() && peer == *testTargetIdentity)
				{
					testConnectionActive = false;
				}
				if (logger)
				{
					logger->WarningFormatted(
						"[EntityHandoff] Closed inactive connection to {} idle={}s",
						peer.ToString(), idleFor.count());
				}
			});
	}

	if (now - lastProbeTime < kProbeInterval)
	{
		return;
	}

	lastProbeTime = now;
	if (!testTargetIdentity.has_value())
	{
		SelectTestTargetShard();
		return;
	}

	if (leaseCoordinator &&
		!leaseCoordinator->TryAcquireOrRefreshLease(*testTargetIdentity))
	{
		if (logger)
		{
			logger->DebugFormatted(
				"[EntityHandoff] Lease held by peer, skipping connect to {}",
				testTargetIdentity->ToString());
		}
		return;
	}

	Interlink::Get().EstablishConnectionTo(*testTargetIdentity);
	HandoffPacketManager::Get().SendPing(*testTargetIdentity);
	testConnectionActive = true;
	if (leaseCoordinator)
	{
		leaseCoordinator->MarkConnectionActivity(*testTargetIdentity);
	}
	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] Probe ping sent to {}",
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
		if (leaseCoordinator)
		{
			leaseCoordinator->ReleaseLeaseIfOwned(*testTargetIdentity);
		}
	}

	testConnectionActive = false;
	testTargetIdentity.reset();
	if (leaseCoordinator)
	{
		leaseCoordinator->Clear();
		leaseCoordinator.reset();
	}
	HandoffPacketManager::Get().Shutdown();
	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] HandoffConnectionManager shutdown");
	}
}

void HandoffConnectionManager::MarkConnectionActivity(const NetworkIdentity& peer)
{
	if (!initialized)
	{
		return;
	}

	if (leaseCoordinator)
	{
		leaseCoordinator->MarkConnectionActivity(peer);
	}
}

void HandoffConnectionManager::SetLeaseModeEnabled(bool enabled)
{
	leaseModeEnabled = enabled;
	if (leaseCoordinator)
	{
		leaseCoordinator->SetLeaseEnabled(enabled);
	}
}
