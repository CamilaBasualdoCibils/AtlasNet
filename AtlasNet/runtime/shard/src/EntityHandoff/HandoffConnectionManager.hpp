#pragma once

#include <chrono>
#include <memory>
#include <optional>

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

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void SelectTestTargetShard();

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	std::optional<NetworkIdentity> testTargetIdentity;
	bool testConnectionActive = false;
	std::chrono::steady_clock::time_point lastToggleTime;
};
