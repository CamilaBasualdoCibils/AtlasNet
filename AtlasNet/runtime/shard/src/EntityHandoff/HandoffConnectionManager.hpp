#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "EntityHandoff/HandoffConnectionLeaseCoordinator.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class HandoffConnectionManager : public Singleton<HandoffConnectionManager>
{
  public:
	HandoffConnectionManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();
	void MarkConnectionActivity(const NetworkIdentity& peer);
	void SetLeaseModeEnabled(bool enabled);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void SelectTestTargetShard();

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	std::optional<NetworkIdentity> testTargetIdentity;
	bool testConnectionActive = false;
	bool leaseModeEnabled = true;
	std::chrono::steady_clock::time_point lastProbeTime;
	std::unique_ptr<HandoffConnectionLeaseCoordinator> leaseCoordinator;
};
