#pragma once

#include <cstdint>
#include <optional>

#include "Debug/Log.hpp"

#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
class ShardService : public Singleton<ShardService>
{
	Log logger = Log("ShardService");
	uint32_t activeShardCount = 0;

   private:
	[[nodiscard]] std::optional<uint32_t> Internal_ResolveDesiredShardReplicaCountKubernetes();
	[[nodiscard]] std::optional<uint32_t> Internal_ResolveDesiredShardReplicaCountDockerSwarm();
	void Internal_ScaleShardServiceKubernetes(uint32_t newShardCount);
	void Internal_ScaleShardServiceDockerSwarm(uint32_t newShardCount);

   public:
	[[nodiscard]] uint32_t ResolveDesiredShardReplicaCount();
	void ScaleShardService(uint32_t newShardCount);
};
