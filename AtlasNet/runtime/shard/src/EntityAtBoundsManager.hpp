#pragma once

#include <atomic>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Entity.hpp"
#include "EntityHandoff.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "pch.hpp"

class EntityAtBoundsManager : public Singleton<EntityAtBoundsManager>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityAtBoundsManager");
	std::atomic_bool ShouldShutdown = false;

	using EntityID = AtlasEntityMinimal::EntityID;

	// Entities we own (in our bounds).
	std::unordered_map<EntityID, AtlasEntity> authoritativeEntities_;
	struct HandoffEntry
	{
		AtlasEntity entity;
		std::string targetPartitionKey;
	};
	// Entities currently in handoff: entity id -> {entity snapshot, target partition key}.
	std::unordered_map<EntityID, HandoffEntry> entitiesInHandoff_;
	// Pending handoff futures; drained each tick to call OnHandoffResult when ready.
	std::vector<std::future<HandoffResult>> pendingHandoffFutures_;

public:
	EntityAtBoundsManager() = default;
	~EntityAtBoundsManager() = default;
	void Init() {}
	void Shutdown() { ShouldShutdown = true; }

	// Processes entities and manages handoff: moves OOB entities to handoff, drains completed futures.
	// Works with internal authoritativeEntities_ map.
	void TickHandoff();

	// Called when a handoff completes. Success: remove from in-handoff. Failure: move back to authoritative.
	void OnHandoffResult(EntityID entityId, bool success);

	// Test helpers
	void InitCircularTestEntity(const glm::vec3& center, float radius);
	// Updates positions of authoritative entities using circular motion.
	void UpdateCircularTestEntities(float deltaTimeSeconds);
	void DebugPrintClaimedBounds();

private:
	static std::string GetSelfPartitionKey();
	std::optional<std::string> FindBoundsForEntity(const AtlasEntity& entity) const;
	// True if partition key (e.g. "eShard <id>") refers to this shard.
	static bool IsPartitionKeySelf(const std::string& partitionKey);

	glm::vec3 circularCenter{0.0f};
	float circularRadius = 0.0f;
	float circularAngle = 0.0f;
	float circularAngularSpeed = 0.5f;
};

