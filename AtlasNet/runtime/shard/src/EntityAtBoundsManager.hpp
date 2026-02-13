#pragma once

#include <atomic>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "Entity.hpp"
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

public:
	EntityAtBoundsManager() = default;
	~EntityAtBoundsManager() = default;
	void Init() {}
	void Shutdown() { ShouldShutdown = true; }

	// Returns entities that are out of our bounds (owner is another partition or none).
	std::vector<AtlasEntity> DetectEntitiesOutOfBounds(ByteWriter& bw);

	// Returns true if the OOB set (entity id -> target) changed; updates internal cache. Call before handoff.
	bool HasOobSetChanged(std::span<const AtlasEntity> oobEntities);

	// Initiates handoff of the given OOB entities (call after HasOobSetChanged returns true).
	void InitiateEntityHandoff(ByteWriter& bw);

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

