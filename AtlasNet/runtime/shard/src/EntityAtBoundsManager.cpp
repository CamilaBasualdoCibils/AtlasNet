#include "EntityAtBoundsManager.hpp"

#include <chrono>
#include <random>
#include <unordered_map>

#include "DockerIO.hpp"
#include "EntityHandoff.hpp"
#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Serialize/ByteReader.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"

std::vector<AtlasEntity> EntityAtBoundsManager::DetectEntitiesOutOfBounds(ByteWriter& bw,
                                                                          std::unordered_set<EntityID>* inBoundsIds)
{
	std::vector<AtlasEntity> out_of_bounds_entities;
	ByteReader br(bw.bytes());

	while (br.remaining() > 0)
	{
		AtlasEntity entity;
		entity.Deserialize(br);

		auto ownerPartition = FindBoundsForEntity(entity);
		// Out of our bounds: no owner or owner is another partition (not us).
		const bool outOfOurBounds =
			!ownerPartition.has_value() || (ownerPartition.has_value() && !IsPartitionKeySelf(*ownerPartition));
		if (outOfOurBounds)
			out_of_bounds_entities.push_back(entity);
		else if (inBoundsIds)
			inBoundsIds->insert(entity.Entity_ID);
	}

	return out_of_bounds_entities;
}

bool EntityAtBoundsManager::IsPartitionKeySelf(const std::string& partitionKey)
{
	auto parsed = InterLinkIdentifier::FromString(partitionKey);
	if (!parsed.has_value())
		return false;
	return std::string(parsed->ID.data()) == GetSelfPartitionKey();
}

std::unordered_map<EntityAtBoundsManager::EntityID, std::string>
EntityAtBoundsManager::BuildOobEntityToTargetMap(std::span<const AtlasEntity> entities) const
{
	const std::string self = GetSelfPartitionKey();
	std::unordered_map<EntityID, std::string> out;
	for (const AtlasEntity& e : entities)
	{
		auto target = FindBoundsForEntity(e);
		if (!target.has_value() || IsPartitionKeySelf(*target))
			continue;
		out[e.Entity_ID] = *target;
	}
	return out;
}

bool EntityAtBoundsManager::OobMapsEqual(const std::unordered_map<EntityID, std::string>& a,
                                         const std::unordered_map<EntityID, std::string>& b)
{
	if (a.size() != b.size())
		return false;
	for (const auto& [eid, target] : a)
	{
		auto it = b.find(eid);
		if (it == b.end() || it->second != target)
			return false;
	}
	return true;
}

bool EntityAtBoundsManager::HasOobSetChanged(std::span<const AtlasEntity> oobEntities)
{
	std::unordered_map<EntityID, std::string> current = BuildOobEntityToTargetMap(oobEntities);
	if (OobMapsEqual(current, previousOobEntityToTarget_))
		return false;
	previousOobEntityToTarget_ = std::move(current);
	return true;
}

std::optional<std::string> EntityAtBoundsManager::FindBoundsForEntity(const AtlasEntity& entity) const
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

void EntityAtBoundsManager::OnHandoffResult(EntityID entityId, bool success)
{
	if (success)
		entitiesInHandoff_.erase(entityId);
	// On failure we keep the entity in entitiesInHandoff_ so we retry on next tick.
}

void EntityAtBoundsManager::TickHandoff(ByteWriter& bw)
{
	// Drain handoff futures that completed (immediately or in the future).
	for (auto it = pendingHandoffFutures_.begin(); it != pendingHandoffFutures_.end();)
	{
		if (it->valid() &&
		    it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				HandoffResult r = it->get();
				OnHandoffResult(r.entityId, r.success);
			}
			catch (...)
			{
				logger->Warning("[EntityAtBoundsManager] Handoff future exception; entity stays in handoff for retry");
			}
			it = pendingHandoffFutures_.erase(it);
		}
		else
			++it;
	}

	authoritativeEntities_.clear();
	std::vector<AtlasEntity> oobEntities =
	    DetectEntitiesOutOfBounds(bw, &authoritativeEntities_);

	std::unordered_map<EntityID, std::string> currentOobMap = BuildOobEntityToTargetMap(oobEntities);
	if (!HasOobSetChanged(oobEntities))
		return;

	entitiesInHandoff_ = currentOobMap;

	std::vector<EntityHandoffRequest> requests;
	requests.reserve(oobEntities.size());
	for (const AtlasEntity& e : oobEntities)
	{
		auto it = entitiesInHandoff_.find(e.Entity_ID);
		if (it == entitiesInHandoff_.end())
			continue;
		requests.push_back(EntityHandoffRequest{ e, it->second });
	}
	if (!requests.empty())
	{
		std::vector<std::future<HandoffResult>> newFutures =
		    EntityHandoff::Get().RequestHandoff(requests);
		for (auto& f : newFutures)
			pendingHandoffFutures_.push_back(std::move(f));
	}
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

	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<AtlasEntityMinimal::EntityID> dist(1, UINT64_MAX);

	circularTestEntity = AtlasEntity{};
	circularTestEntity.Entity_ID = dist(gen);
	circularTestEntity.transform.position = center + glm::vec3(radius, 0.0f, 0.0f);
	circularTestEntity.IsClient = false;
	circularTestEntity.Client_ID = 0;
	circularTestEntity.Metadata.clear();

	hasCircularTestEntity = true;

	logger->DebugFormatted(
		"[EntityAtBoundsManager] Initialized circular test entity id={} at center ({}, {}, {}), radius {}",
		circularTestEntity.Entity_ID,
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