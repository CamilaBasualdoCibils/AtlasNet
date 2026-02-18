#include "EntityHandoff/EntityAuthorityManager.hpp"
#include "EntityHandoff/HandoffConnectionManager.hpp"

void EntityAuthorityManager::Init(const NetworkIdentity& self,
								  std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;

	HandoffConnectionManager::Get().Init(selfIdentity, logger);

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] EntityAuthorityManager initialized for {}",
			selfIdentity.ToString());
	}
}

void EntityAuthorityManager::Tick() const
{
	if (!initialized)
	{
		return;
	}

	// Stub: authority graph, handoff queue, and tick-based transitions.
	HandoffConnectionManager::Get().Tick();
}

void EntityAuthorityManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	HandoffConnectionManager::Get().Shutdown();
	initialized = false;

	if (logger)
	{
		logger->Debug("[EntityHandoff] EntityAuthorityManager shutdown");
	}
}
