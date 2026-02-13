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
	// Target partition id -> entity IDs we have an open handoff for (we initiated the connection).
	std::unordered_map<std::string, std::unordered_set<EntityID>> openHandoffsByTarget_;

	std::string getSelfPartitionKey() const;
	std::optional<std::string> resolveTargetPartition(const AtlasEntity& entity) const;
	static InterLinkIdentifier partitionKeyToIdentifier(const std::string& partitionKey);
	static bool shouldInitiateConnectionTo(const std::string& selfPartitionKey, const std::string& targetPartitionKey);

public:
	EntityHandoff() = default;
	~EntityHandoff() = default;

	void Init() {}
	void Shutdown() {}

	/**
	 * Perform handoff for the given OOB entities: open/close connections to target shards as needed.
	 * Call only when the OOB set has changed (EntityAtBoundsManager::HasOobSetChanged).
	 */
	void InitiateHandoff(std::span<const AtlasEntity> entities);

	/**
	 * Deserialize entities from bw and call InitiateHandoff with them.
	 */
	void InitiateHandoffFromSerialized(ByteWriter& bw);

private:
	void updateConnectionsForCurrentOob(std::span<const AtlasEntity> entities);
};
