#pragma once

#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Entity.hpp"
#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"

/** One entity and the partition key it should be handed off to. */
struct EntityHandoffRequest
{
	AtlasEntity entity;
	std::string targetPartitionKey;
};

/**
 * Performs entity handoff: ensures connection to target shard and notifies the
 * manager when each handoff succeeds or fails (callback may be invoked async in the future).
 */
class EntityHandoff : public Singleton<EntityHandoff>
{
	using EntityID = AtlasEntityMinimal::EntityID;
	using HandoffResultCallback = std::function<void(EntityID entityId, bool success)>;

	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityHandoff");
	std::unordered_map<std::string, std::unordered_set<EntityID>> openConnectionsByTarget_;
	HandoffResultCallback resultCallback_;

	static InterLinkIdentifier partitionKeyToIdentifier(const std::string& partitionKey);
	static bool shouldInitiateConnectionTo(const std::string& selfPartitionKey,
	                                       const std::string& targetPartitionKey);
	std::string getSelfPartitionKey() const;

public:
	EntityHandoff() = default;
	~EntityHandoff() = default;

	void Init() {}
	void Shutdown() {}

	/** Set callback for handoff result. Called with (entityId, success). */
	void SetResultCallback(HandoffResultCallback cb) { resultCallback_ = std::move(cb); }

	/**
	 * Request handoff for the given (entity, target) pairs.
	 * Ensures connection to each target; invokes resultCallback_(entityId, true/false) per entity.
	 */
	void RequestHandoff(std::span<const EntityHandoffRequest> requests);

private:
	void ensureConnectionsAndNotify(std::span<const EntityHandoffRequest> requests);
};
