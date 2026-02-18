#include "EntityAtBoundsManager.hpp"

#include <chrono>
#include <random>
#include <unordered_map>

#include "DockerIO.hpp"
#include "EntityHandoff.hpp"
#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"


bool EntityAtBoundsManager::IsPartitionKeySelf(const std::string& partitionKey)
{
	auto parsed = InterLinkIdentifier::FromString(partitionKey);
	if (!parsed.has_value())
		return false;
	return std::string(parsed->ID.data()) == GetSelfPartitionKey();
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
	auto it = entitiesInHandoff_.find(entityId);
	if (it == entitiesInHandoff_.end())
		return;

	if (success)
	{
		// Handoff succeeded: remove from in-handoff (entity is now owned by target shard).
		entitiesInHandoff_.erase(it);
	}
	else
	{
		// Handoff failed: move back to authoritative (we still own it, will retry later).
		authoritativeEntities_[entityId] = it->second.entity;
		entitiesInHandoff_.erase(it);
	}
}

void EntityAtBoundsManager::TickHandoff()
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

	// Process all authoritative entities: determine which are OOB and need handoff.
	std::vector<EntityHandoffRequest> requests;
	
	for (auto it = authoritativeEntities_.begin(); it != authoritativeEntities_.end();)
	{
		const AtlasEntity& entity = it->second;
		auto ownerPartition = FindBoundsForEntity(entity);
		const bool inOurBounds = ownerPartition.has_value() && IsPartitionKeySelf(*ownerPartition);

		if (!inOurBounds && ownerPartition.has_value())
		{
			// Entity is OOB and has a target partition: move to handoff and request handoff.
			// Only request if not already in handoff (avoid duplicate requests).
			if (entitiesInHandoff_.find(entity.Entity_ID) == entitiesInHandoff_.end())
			{
				entitiesInHandoff_[entity.Entity_ID] = HandoffEntry{ entity, *ownerPartition };
				requests.push_back(EntityHandoffRequest{ entity, *ownerPartition });
			}
			else
			{
				// Keep latest snapshot while waiting for handoff result.
				entitiesInHandoff_[entity.Entity_ID] = HandoffEntry{ entity, *ownerPartition };
			}
			// Remove from authoritative since it's OOB.
			it = authoritativeEntities_.erase(it);
		}
		else if (!inOurBounds)
		{
			// Entity is OOB but has no target (nowhere to hand off). Keep in authoritative for now.
			++it;
		}
		else
		{
			// Entity is in our bounds: keep authoritative. Remove from handoff if it was there.
			entitiesInHandoff_.erase(entity.Entity_ID);
			++it;
		}
	}

	// Request handoff for entities we just moved to handoff.
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

	AtlasEntity entity{};
	entity.Entity_ID = dist(gen);
	entity.transform.position = center + glm::vec3(radius, 0.0f, 0.0f);
	entity.IsClient = false;
	entity.Client_ID = 0;
	entity.Metadata.clear();
	authoritativeEntities_[entity.Entity_ID] = entity;

	logger->DebugFormatted(
		"[EntityAtBoundsManager] Initialized circular test entity id={} at center ({}, {}, {}), radius {}",
		entity.Entity_ID,
		center.x,
		center.y,
		center.z,
		radius);
}

void EntityAtBoundsManager::UpdateCircularTestEntities(float deltaTimeSeconds)
{

	if (authoritativeEntities_.size() > 0)
	{
		logger->DebugFormatted("entities updated: {}", authoritativeEntities_.size());
	}

	if (entitiesInHandoff_.size() > 0)
	{
		logger->DebugFormatted("entities handing off: {}", entitiesInHandoff_.size());
	}


	if (authoritativeEntities_.empty())
		return;

	// Advance all authoritative test entities along a circular path.
	circularAngle += circularAngularSpeed * deltaTimeSeconds;

	const size_t count = authoritativeEntities_.size();
	size_t idx = 0;
	for (auto& [entityId, entity] : authoritativeEntities_)
	{
		(void)entityId;
		const float phase = circularAngle + (static_cast<float>(idx) * 6.28318530718f / static_cast<float>(count));
		const float x = circularCenter.x + circularRadius * std::cos(phase);
		const float y = circularCenter.y + circularRadius * std::sin(phase);
		const float z = circularCenter.z;
		entity.transform.position = glm::vec3(x, y, z);
		++idx;

	}
}
