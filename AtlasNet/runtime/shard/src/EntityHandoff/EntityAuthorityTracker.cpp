#include "EntityHandoff/EntityAuthorityTracker.hpp"

#include <unordered_set>

EntityAuthorityTracker::EntityAuthorityTracker(const NetworkIdentity& self,
											   std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void EntityAuthorityTracker::Reset()
{
	authorityByEntityId.clear();
}

void EntityAuthorityTracker::SetOwnedEntities(
	const std::vector<AtlasEntity>& ownedEntities)
{
	std::unordered_set<AtlasEntity::EntityID> incoming;
	incoming.reserve(ownedEntities.size());
	for (const auto& entity : ownedEntities)
	{
		incoming.insert(entity.Entity_ID);
	}
	for (auto entityIt = authorityByEntityId.begin();
		 entityIt != authorityByEntityId.end();)
	{
		if (incoming.contains(entityIt->first))
		{
			++entityIt;
			continue;
		}
		entityIt = authorityByEntityId.erase(entityIt);
	}

	for (const auto& entity : ownedEntities)
	{
		auto& entry = authorityByEntityId[entity.Entity_ID];
		entry.entitySnapshot = entity;
		if (!entry.passingTo.has_value() &&
			entry.authorityState != AuthorityState::ePassing)
		{
			entry.authorityState = AuthorityState::eAuthoritative;
		}
	}
}

void EntityAuthorityTracker::CollectTelemetryRows(
	std::vector<AuthorityTelemetryRow>& outRows) const
{
	outRows.clear();
	outRows.reserve(authorityByEntityId.size());
	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		outRows.push_back(AuthorityTelemetryRow{
			.entityId = entityId,
			.owner = selfIdentity,
			.entitySnapshot = entry.entitySnapshot,
			.world = entry.entitySnapshot.transform.world,
			.position = entry.entitySnapshot.transform.position,
			.isClient = entry.entitySnapshot.IsClient,
			.clientId = entry.entitySnapshot.Client_ID});
	}
}

void EntityAuthorityTracker::SetAuthorityState(
	AtlasEntity::EntityID entityId, AuthorityState state,
	const std::optional<NetworkIdentity>& passingTo)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return;
	}

	entityIt->second.authorityState = state;
	entityIt->second.passingTo = passingTo;
}

void EntityAuthorityTracker::RemoveEntity(AtlasEntity::EntityID entityId)
{
	authorityByEntityId.erase(entityId);
}

bool EntityAuthorityTracker::MarkPassing(
	AtlasEntity::EntityID entityId, const NetworkIdentity& passingTarget)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return false;
	}

	const bool alreadyPassingToTarget =
		entityIt->second.authorityState == AuthorityState::ePassing &&
		entityIt->second.passingTo.has_value() &&
		entityIt->second.passingTo.value() == passingTarget;
	entityIt->second.authorityState = AuthorityState::ePassing;
	entityIt->second.passingTo = passingTarget;
	return !alreadyPassingToTarget;
}

void EntityAuthorityTracker::MarkAuthoritative(AtlasEntity::EntityID entityId)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return;
	}

	entityIt->second.authorityState = AuthorityState::eAuthoritative;
	entityIt->second.passingTo.reset();
}

void EntityAuthorityTracker::DebugLogTrackedEntities() const
{
	if (!logger)
	{
		return;
	}

	if (authorityByEntityId.size() > 0)
	{
		logger->DebugFormatted("[EntityHandoff] AuthorityTracker owns {} entities", authorityByEntityId.size());
	}


	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		const char* state =
			entry.authorityState == AuthorityState::eAuthoritative
				? "authoritative"
				: "passing";
		const std::string passingTo =
			entry.passingTo.has_value() ? entry.passingTo->ToString() : "-";
		const auto& pos = entry.entitySnapshot.transform.position;
		logger->DebugFormatted(
			"[EntityHandoff]  entity={} pos=({}, {}, {}) state={} passing_to={} world={}",
			entityId, pos[0], pos[1], pos[2], state, passingTo, entry.entitySnapshot.transform.world);
	}
}

std::vector<AtlasEntity> EntityAuthorityTracker::GetOwnedEntitySnapshots() const
{
	std::vector<AtlasEntity> entities;
	entities.reserve(authorityByEntityId.size());
	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		(void)entityId;
		entities.push_back(entry.entitySnapshot);
	}
	return entities;
}
