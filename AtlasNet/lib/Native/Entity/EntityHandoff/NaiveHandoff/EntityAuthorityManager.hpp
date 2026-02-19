#pragma once

// Thin facade for shard-side authority lifecycle. Runtime details are delegated
// to an internal component so this manager only forwards init/tick/shutdown.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;

class EntityAuthorityManager : public Singleton<EntityAuthorityManager>
{
  public:
	struct PendingIncomingHandoff
	{
		AtlasEntity entity;
		NetworkIdentity sender;
		uint64_t transferTick = 0;
	};

	struct PendingOutgoingHandoff
	{
		AtlasEntityID entityId = 0;
		NetworkIdentity targetIdentity;
		std::string targetClaimKey;
		uint64_t transferTick = 0;
	};

	EntityAuthorityManager();
	~EntityAuthorityManager();

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);
	void OnIncomingHandoffEntityAtTick(const AtlasEntity& entity,
									   const NetworkIdentity& sender,
									   uint64_t transferTick);

	[[nodiscard]] bool IsInitialized() const;

  private:
	class Runtime;
	std::unique_ptr<Runtime> runtime;
};
