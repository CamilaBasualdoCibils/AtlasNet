#pragma once

#include <memory>

#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class EntityAuthorityManager : public Singleton<EntityAuthorityManager>
{
  public:
	EntityAuthorityManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick() const;
	void Shutdown();

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
};
