#pragma once

#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Entity.hpp"
#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Serialize/ByteWriter.hpp"

/**
 * Handles entity handoff for the spatial server mesh: when an entity leaves
 * this partition's bounds we open an Interlink connection to the target shard
 * (for verification); when the entity is back in bounds we close that connection.
 */
class EntityHandoff : public Singleton<EntityHandoff>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityHandoff");

	using EntityID = AtlasEntityMinimal::EntityID;
	// Target partition id (shard container name) -> entity IDs we have an open handoff for.
	std::unordered_map<std::string, std::unordered_set<EntityID>> openHandoffsByTarget_;
	// Last OOB set (entity id -> target): only run open/close when this changes.
	std::unordered_map<EntityID, std::string> previousOobEntityToTarget_;

	std::string getSelfPartitionKey() const;
	std::optional<std::string> resolveTargetPartition(const AtlasEntity& entity) const;
	/// Partition key from HeuristicManifest is full ToString() e.g. "eShard 37d8dbb86abc". Parse so ID matches registry.
	static InterLinkIdentifier partitionKeyToIdentifier(const std::string& partitionKey);
	/// Only we open/close to target when we are the canonical initiator (self < target by container id). Avoids both sides opening.
	static bool shouldInitiateConnectionTo(const std::string& selfPartitionKey, const std::string& targetPartitionKey);
	// Build map entity_id -> target for entities that are OOB (and have a valid target != self).
	std::unordered_map<EntityID, std::string> buildOobEntityToTarget(std::span<const AtlasEntity> entities) const;
	static bool oobMapsEqual(const std::unordered_map<EntityID, std::string>& a,
	                         const std::unordered_map<EntityID, std::string>& b);

public:
	EntityHandoff() = default;
	~EntityHandoff() = default;

	void Init() {}
	void Shutdown() {}

	/**
	 * Update handoff state from the current list of entities that are out of our bounds.
	 * Opens an Interlink connection to each target shard when an entity goes OOB;
	 * closes the connection when no entity is OOB for that target anymore.
	 */
	void InitiateHandoff(std::span<const AtlasEntity> entities);

	/**
	 * Deserialize entities from bw and call InitiateHandoff with them.
	 */
	void InitiateHandoffFromSerialized(ByteWriter& bw);

private:
	void updateConnectionsForCurrentOob(std::span<const AtlasEntity> entities);
};
