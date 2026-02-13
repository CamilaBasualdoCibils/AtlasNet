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
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"

class EntityAtBoundsManager : public Singleton<EntityAtBoundsManager>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityAtBoundsManager");
	std::atomic_bool ShouldShutdown = false;

	using EntityID = AtlasEntityMinimal::EntityID;
	// Cache: entity id -> target partition key. Used to only trigger handoff when OOB set changes.
	std::unordered_map<EntityID, std::string> previousOobEntityToTarget_;

	// Entities we own (in our bounds).
	std::unordered_set<EntityID> authoritativeEntities_;
	// Entities currently in handoff: entity id -> target partition key. Handoff notifies via futures.
	std::unordered_map<EntityID, std::string> entitiesInHandoff_;
	// Pending handoff futures; drained each tick to call OnHandoffResult when ready.
	std::vector<std::future<HandoffResult>> pendingHandoffFutures_;

public:
	EntityAtBoundsManager() = default;
	~EntityAtBoundsManager() = default;
	void Init() {}
	void Shutdown() { ShouldShutdown = true; }

	// Returns entities that are out of our bounds. Optionally fills inBoundsIds with entity ids that are in our bounds.
	std::vector<AtlasEntity> DetectEntitiesOutOfBounds(ByteWriter& bw,
	                                                   std::unordered_set<EntityID>* inBoundsIds = nullptr);

	// Returns true if the OOB set (entity id -> target) changed; updates internal cache. Call before handoff.
	bool HasOobSetChanged(std::span<const AtlasEntity> oobEntities);

	// Builds handoff requests from current OOB set and calls Handoff; updates authoritative / in-handoff maps.
	void TickHandoff(ByteWriter& bw);

	// Called by Handoff when a handoff completes (success or failure). Success: remove from in-handoff; failure: keep for retry.
	void OnHandoffResult(EntityID entityId, bool success);

	// Test helpers
	void InitCircularTestEntity(const glm::vec3& center, float radius);
	void UpdateAndSerializeCircularTestEntity(ByteWriter& bw, float deltaTimeSeconds);
	void DebugPrintClaimedBounds();

private:
	static std::string GetSelfPartitionKey();
	std::optional<std::string> FindBoundsForEntity(const AtlasEntity& entity) const;
	// True if partition key (e.g. "eShard <id>") refers to this shard.
	static bool IsPartitionKeySelf(const std::string& partitionKey);
	// Build entity id -> target partition key for entities that are OOB with a valid target != self.
	std::unordered_map<EntityID, std::string> BuildOobEntityToTargetMap(std::span<const AtlasEntity> entities) const;
	static bool OobMapsEqual(const std::unordered_map<EntityID, std::string>& a,
	                         const std::unordered_map<EntityID, std::string>& b);

	void PrintEntityToBoundsInfo(const AtlasEntity& entity,
	                             const std::optional<std::string>& ownerPartition);

	bool hasCircularTestEntity = false;
	AtlasEntity circularTestEntity{};
	glm::vec3 circularCenter{0.0f};
	float circularRadius = 0.0f;
	float circularAngle = 0.0f;
	float circularAngularSpeed = 0.5f;
};

