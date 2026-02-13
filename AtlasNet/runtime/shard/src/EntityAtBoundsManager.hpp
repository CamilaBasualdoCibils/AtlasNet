#pragma once

#include <atomic>
#include <optional>
#include <string>

#include "Misc/Singleton.hpp"
#include "pch.hpp"
#include "Log.hpp"
#include "Entity.hpp"
#include "Serialize/ByteWriter.hpp"

class EntityAtBoundsManager : public Singleton<EntityAtBoundsManager>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityAtBoundsManager");
	std::atomic_bool ShouldShutdown = false;

public:
	EntityAtBoundsManager() = default;
	~EntityAtBoundsManager() {};
	void Init() {};
	void Shutdown() {ShouldShutdown = true;}

  // Returns a vector of entities that are out of bounds, and should be handed off to another shard.
  std::vector<AtlasEntity> DetectEntitiesOutOfBounds(ByteWriter& bw);

  // Initiates the handoff of entities that are out of bounds to another shard.
  void InitiateEntityHandoff(ByteWriter& bw);


  // Test helpers
  // Initializes a single test entity that will move in a circle.
  void InitCircularTestEntity(const glm::vec3& center, float radius);
  void UpdateAndSerializeCircularTestEntity(ByteWriter& bw, float deltaTimeSeconds);
  // Debug helper: print all claimed bounds and their owners.
  void DebugPrintClaimedBounds();

private:
  // Returns this shard's partition key (container id).
  static std::string GetSelfPartitionKey();
  // Returns the partition key (e.g. shard/container id) that owns this entity's position, if any.
  std::optional<std::string> FindBoundsForEntity(const AtlasEntity& entity);

  // Debug helpers: print entity and its bounds ownership info.
  void PrintEntityToBoundsInfo(const AtlasEntity& entity, const std::optional<std::string>& ownerPartition);

  // Debug helpers: State for the circular test entity
  bool hasCircularTestEntity = false;
  AtlasEntity circularTestEntity{};
  glm::vec3 circularCenter{0.0f};
  float circularRadius = 0.0f;
  float circularAngle = 0.0f;          // radians
  float circularAngularSpeed = 0.5f;   // radians per second (test value)
};

