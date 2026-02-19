// Implements deterministic debug entity spawning, orbit updates, and snapshot writes.

#include "Entity/EntityHandoff/DebugEntityOrbitSimulator.hpp"

#include <algorithm>
#include <cmath>
#include "Entity/Entity.hpp"

namespace
{
AtlasEntityID MakeEntityId(uint32_t index)
{
	// Keep debug entity IDs shard-agnostic so ownership transfer does not create
	// a second synthetic entity with a different ID on the receiving shard.
	static constexpr AtlasEntityID kDebugEntityIdNamespace =
		static_cast<AtlasEntityID>(0xA7105EED00000000ULL);
	return kDebugEntityIdNamespace ^
		   (static_cast<AtlasEntityID>(index + 1U) << 1U);
}
}  // namespace

DebugEntityOrbitSimulator::DebugEntityOrbitSimulator(const NetworkIdentity& self,
													 std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void DebugEntityOrbitSimulator::Reset()
{
	entitiesById.clear();
	orbitAngleRad = 0.0F;
}

void DebugEntityOrbitSimulator::SeedEntities(const SeedOptions& options)
{
	if (entitiesById.size() >= options.desiredCount)
	{
		return;
	}

	for (uint32_t i = static_cast<uint32_t>(entitiesById.size());
		 i < options.desiredCount; ++i)
	{
		AtlasEntity entity;
		entity.Entity_ID = MakeEntityId(i);
		entity.transform.world = 0;
		entity.transform.position = vec3(0.0F, 0.0F, 0.0F);
		entity.transform.boundingBox.SetCenterExtents(
			entity.transform.position,
			vec3(options.halfExtent, options.halfExtent, options.halfExtent));
		entity.IsClient = false;
		entity.Client_ID = UUID();
		entity.Metadata.clear();

		OrbitEntity orbitEntity;
		orbitEntity.entity = entity;
		orbitEntity.phaseOffsetRad = options.phaseStepRad * static_cast<float>(i);
		entitiesById[entity.Entity_ID] = orbitEntity;
	}
}

void DebugEntityOrbitSimulator::AdoptSingleEntity(const AtlasEntity& entity)
{
	OrbitEntity orbitEntity;
	orbitEntity.entity = entity;
	const vec3 position = entity.transform.position;
	if (std::abs(position.x) > 1e-4F || std::abs(position.y) > 1e-4F)
	{
		const float angleRad = std::atan2(position.y, position.x);
		orbitEntity.phaseOffsetRad = angleRad - orbitAngleRad;
	}
	else
	{
		orbitEntity.phaseOffsetRad = 0.0F;
	}
	entitiesById[entity.Entity_ID] = orbitEntity;
}

void DebugEntityOrbitSimulator::TickOrbit(const OrbitOptions& options)
{
	const float deltaSeconds = std::clamp(options.deltaSeconds, 0.0F, 0.25F);
	orbitAngleRad += deltaSeconds * options.angularSpeedRadPerSec;
	for (auto& [entityId, orbitEntity] : entitiesById)
	{
		(void)entityId;
		const float angle = orbitAngleRad + orbitEntity.phaseOffsetRad;
		const vec3 position = vec3(std::cos(angle) * options.radius,
								   std::sin(angle) * options.radius,
								   orbitEntity.entity.transform.position.z);
		orbitEntity.entity.transform.position = position;
		orbitEntity.entity.transform.boundingBox.SetCenterExtents(
			position,
			vec3(kDefaultHalfExtent, kDefaultHalfExtent, kDefaultHalfExtent));
	}
}

void DebugEntityOrbitSimulator::StoreStateSnapshots(std::string_view hashKey) const
{
	(void)hashKey;
}

std::vector<AtlasEntity> DebugEntityOrbitSimulator::GetEntitiesSnapshot() const
{
	std::vector<AtlasEntity> snapshots;
	snapshots.reserve(entitiesById.size());
	for (const auto& [_entityId, orbitEntity] : entitiesById)
	{
		snapshots.push_back(orbitEntity.entity);
	}
	return snapshots;
}
