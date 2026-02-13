#include "EntityAtBoundsManager.hpp"

#include <unordered_map>

#include "DockerIO.hpp"
#include "EntityHandoff.hpp"
#include "Log.hpp"
#include "Serialize/ByteReader.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"

std::vector<AtlasEntity> EntityAtBoundsManager::DetectEntitiesOutOfBounds(ByteWriter& bw)
{
	std::vector<AtlasEntity> out_of_bounds_entities;
	ByteReader br(bw.bytes());
	const std::string selfPartitionKey = GetSelfPartitionKey();

	while (br.remaining() > 0)
	{
		AtlasEntity entity;
		entity.Deserialize(br);

		auto ownerPartition = FindBoundsForEntity(entity);
		//PrintEntityToBoundsInfo(entity, ownerPartition);

		// Out of our bounds: owner is another partition or none (entity left our region).
		const bool outOfOurBounds =
			!ownerPartition.has_value() || (ownerPartition.has_value() && *ownerPartition != selfPartitionKey);
		if (outOfOurBounds)
			out_of_bounds_entities.push_back(entity);
	}

	return out_of_bounds_entities;
}

std::optional<std::string> EntityAtBoundsManager::FindBoundsForEntity(const AtlasEntity& entity)
{
	using BoundsMap = std::unordered_map<std::string, GridShape>;

	BoundsMap claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape, std::string>(claimedBounds);

	if (claimedBounds.empty())
	{
		logger->Warning("[EntityAtBoundsManager] No claimed bounds available when resolving entity "
						"ownership");
		return std::nullopt;
	}

	const vec3 pos = entity.transform.position;

	// Check which (if any) claimed bounds contain the entity's position.
	for (const auto& [partitionKey, shape] : claimedBounds)
	{
		if (shape.Contains(pos))
		{
			return partitionKey;
		}
	}

	// Not inside any known claimed bounds.
	return std::nullopt;
}

std::string EntityAtBoundsManager::GetSelfPartitionKey()
{
	return DockerIO::Get().GetSelfContainerName();
}

void EntityAtBoundsManager::InitiateEntityHandoff(ByteWriter& bw)
{
	EntityHandoff::Get().InitiateHandoffFromSerialized(bw);
}

void EntityAtBoundsManager::DebugPrintClaimedBounds()
{
	using BoundsMap = std::unordered_map<std::string, GridShape>;

	BoundsMap claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape, std::string>(claimedBounds);

	if (claimedBounds.empty())
	{
		logger->Warning("[EntityAtBoundsManager] DebugPrintClaimedBounds: no claimed bounds found");
		return;
	}

	for (const auto& [partitionKey, shape] : claimedBounds)
	{
		const auto id = shape.GetID();
		const auto& min = shape.min;
		const auto& max = shape.max;

		logger->DebugFormatted(
			"[EntityAtBoundsManager] Claimed bound ID {} owned by \"{}\": "
			"min ({}, {}, {}), max ({}, {}, {})",
			id,
			partitionKey,
			min.x, min.y, min.z,
			max.x, max.y, max.z);
	}
}


void EntityAtBoundsManager::InitCircularTestEntity(const glm::vec3& center, float radius)
{
	circularCenter = center;
	circularRadius = radius;
	circularAngle = 0.0f;

	circularTestEntity = AtlasEntity{};
	circularTestEntity.Entity_ID = 1;
	circularTestEntity.transform.position = center + glm::vec3(radius, 0.0f, 0.0f);
	circularTestEntity.IsClient = false;
	circularTestEntity.Client_ID = 0;
	circularTestEntity.Metadata.clear();

	hasCircularTestEntity = true;

	logger->DebugFormatted(
		"[EntityAtBoundsManager] Initialized circular test entity at center ({}, {}, {}), radius {}",
		center.x,
		center.y,
		center.z,
		radius);
}

void EntityAtBoundsManager::UpdateAndSerializeCircularTestEntity(ByteWriter& bw, float deltaTimeSeconds)
{
	if (!hasCircularTestEntity)
	{
		return;
	}

	// Advance along the circle.
	circularAngle += circularAngularSpeed * deltaTimeSeconds;

	const float x = circularCenter.x + circularRadius * std::cos(circularAngle);
	const float y = circularCenter.y + circularRadius * std::sin(circularAngle);
	const float z = circularCenter.z;

	circularTestEntity.transform.position = glm::vec3(x, y, z);

	// Serialize the single moving entity into the provided writer.
	bw.clear();
	circularTestEntity.Serialize(bw);
}

void EntityAtBoundsManager::PrintEntityToBoundsInfo(const AtlasEntity& entity, const std::optional<std::string>& ownerPartition)
{
	if (ownerPartition.has_value())
	{
		logger->DebugFormatted(
			"Entity {} at ({}, {}, {}) belongs to partition \"{}\"",
			entity.Entity_ID,
			entity.transform.position.x,
			entity.transform.position.y,
			entity.transform.position.z,
			*ownerPartition);
	}
	else
	{
		logger->DebugFormatted(
			"Entity {} at ({}, {}, {}) is OUT of all bounds",
			entity.Entity_ID,
			entity.transform.position.x,
			entity.transform.position.y,
			entity.transform.position.z);
	}
}