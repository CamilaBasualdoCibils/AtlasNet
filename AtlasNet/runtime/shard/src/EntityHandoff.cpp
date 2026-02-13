#include "EntityHandoff.hpp"

#include <optional>

#include "DockerIO.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Database/ServerRegistry.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink.hpp"
#include "InterlinkIdentifier.hpp"
#include "Serialize/ByteReader.hpp"

std::string EntityHandoff::getSelfPartitionKey() const
{
	return DockerIO::Get().GetSelfContainerName();
}

std::optional<std::string> EntityHandoff::resolveTargetPartition(const AtlasEntity& entity) const
{
	using BoundsMap = std::unordered_map<std::string, GridShape>;
	BoundsMap claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape, std::string>(claimedBounds);
	const vec3 pos = entity.transform.position;
	for (const auto& [partitionKey, shape] : claimedBounds)
	{
		if (shape.Contains(pos))
			return partitionKey;
	}
	return std::nullopt;
}

std::unordered_map<EntityHandoff::EntityID, std::string> EntityHandoff::buildOobEntityToTarget(
	std::span<const AtlasEntity> entities) const
{
	const std::string self = getSelfPartitionKey();
	std::unordered_map<EntityID, std::string> out;
	for (const AtlasEntity& e : entities)
	{
		auto target = resolveTargetPartition(e);
		if (!target.has_value())
			continue;
		// Partition key is "eShard <container_id>"; self is container id. Skip when target is us.
		auto parsed = InterLinkIdentifier::FromString(*target);
		if (parsed.has_value() && std::string(parsed->ID.data()) == self)
			continue;
		out[e.Entity_ID] = *target;
	}
	return out;
}

InterLinkIdentifier EntityHandoff::partitionKeyToIdentifier(const std::string& partitionKey)
{
	// Partition keys from HeuristicManifest are full ToString() e.g. "eShard 37d8dbb86abc".
	// FromString parses that to Type + ID so ToString() matches what ServerRegistry uses.
	auto parsed = InterLinkIdentifier::FromString(partitionKey);
	if (parsed.has_value())
		return *parsed;
	// Fallback: if key is just container id, MakeIDShard(key) gives correct "eShard <id>".
	return InterLinkIdentifier::MakeIDShard(partitionKey);
}

bool EntityHandoff::oobMapsEqual(const std::unordered_map<EntityID, std::string>& a,
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

void EntityHandoff::updateConnectionsForCurrentOob(std::span<const AtlasEntity> entities)
{
	const std::string self = getSelfPartitionKey();
	std::unordered_map<std::string, std::unordered_set<EntityID>> currentByTarget;

	for (const AtlasEntity& e : entities)
	{
		auto target = resolveTargetPartition(e);
		if (!target.has_value())
			continue;
		auto parsed = InterLinkIdentifier::FromString(*target);
		if (parsed.has_value() && std::string(parsed->ID.data()) == self)
			continue;
		currentByTarget[*target].insert(e.Entity_ID);
	}

	// Close connections for targets that no longer have any OOB entity.
	for (auto it = openHandoffsByTarget_.begin(); it != openHandoffsByTarget_.end();)
	{
		const std::string& target = it->first;
		auto curIt = currentByTarget.find(target);
		if (curIt == currentByTarget.end() || curIt->second.empty())
		{
			InterLinkIdentifier targetId = partitionKeyToIdentifier(target);
			Interlink::Get().CloseConnectionTo(targetId, 0, "entity back in bounds");
			logger->DebugFormatted("EntityHandoff: closed connection to {} (entity back in bounds)", target);
			it = openHandoffsByTarget_.erase(it);
		}
		else
			++it;
	}

	// Open connection for any target we have OOB entities for that we didn't have before.
	// Only add to openHandoffsByTarget_ when we actually have a connection (or had one).
	std::unordered_map<std::string, std::unordered_set<EntityID>> newOpenHandoffs;
	for (const auto& [target, entityIds] : currentByTarget)
	{
		if (entityIds.empty())
			continue;
		bool alreadyHad = (openHandoffsByTarget_.find(target) != openHandoffsByTarget_.end());
		if (!alreadyHad)
		{
			InterLinkIdentifier targetId = partitionKeyToIdentifier(target);
			if (!ServerRegistry::Get().ExistsInRegistry(targetId))
			{
				logger->DebugFormatted("EntityHandoff: target {} not in registry yet, skipping connect", target);
				continue;
			}
			if (!Interlink::Get().EstablishConnectionTo(targetId))
			{
				logger->WarningFormatted("EntityHandoff: failed to open connection to {}", target);
				continue;
			}
			logger->DebugFormatted("EntityHandoff: opened connection to {} (entity out of bounds)", target);
		}
		newOpenHandoffs[target] = entityIds;
	}
	openHandoffsByTarget_ = std::move(newOpenHandoffs);
}

void EntityHandoff::InitiateHandoff(std::span<const AtlasEntity> entities)
{
	std::unordered_map<EntityID, std::string> current = buildOobEntityToTarget(entities);
	// Only run open/close when the OOB set changed (entity just left or just came back).
	if (oobMapsEqual(current, previousOobEntityToTarget_))
		return;
	previousOobEntityToTarget_ = std::move(current);
	updateConnectionsForCurrentOob(entities);
}

void EntityHandoff::InitiateHandoffFromSerialized(ByteWriter& bw)
{
	std::vector<AtlasEntity> entities;
	ByteReader br(bw.bytes());
	while (br.remaining() > 0)
	{
		AtlasEntity entity;
		entity.Deserialize(br);
		entities.push_back(entity);
	}
	InitiateHandoff(entities);
}
