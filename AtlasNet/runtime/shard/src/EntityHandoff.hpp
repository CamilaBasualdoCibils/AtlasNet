#pragma once

#include <future>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

/** Result of a single handoff: completed immediately on failure or in the future on success (e.g. when connection is up). */
struct HandoffResult
{
	AtlasEntityMinimal::EntityID entityId{};
	bool success = false;
};

/**
 * Performs entity handoff: ensures connection to target shard. Each handoff completes via a
 * std::future—either immediately on failure or in the future on success (e.g. when the connection
 * is established; that path is stubbed for now).
 */
class EntityHandoff : public Singleton<EntityHandoff>
{
	using EntityID = AtlasEntityMinimal::EntityID;

	std::shared_ptr<Log> logger = std::make_shared<Log>("EntityHandoff");
	std::unordered_map<std::string, std::unordered_set<EntityID>> openConnectionsByTarget_;
	// Pending promises keyed by target partition key: (promise, entityId) pairs to complete when connection is established.
	using PendingPromise = std::pair<std::shared_ptr<std::promise<HandoffResult>>, EntityID>;
	std::unordered_map<std::string, std::vector<PendingPromise>> pendingPromisesByTarget_;

	static InterLinkIdentifier partitionKeyToIdentifier(const std::string& partitionKey);
	static bool shouldInitiateConnectionTo(const std::string& selfPartitionKey,
	                                       const std::string& targetPartitionKey);
	std::string getSelfPartitionKey() const;
	// Complete promises for entities waiting on this connection target.
	void CompletePromisesForTarget(const std::string& targetPartitionKey);

public:
	EntityHandoff() = default;
	~EntityHandoff() = default;

	void Init() {}
	void Shutdown() {}

	/**
	 * Request handoff for the given (entity, target) pairs. Returns one future per request; each
	 * future is set immediately on failure, or when connection is established on success.
	 */
	std::vector<std::future<HandoffResult>> RequestHandoff(std::span<const EntityHandoffRequest> requests);

	/**
	 * Called when a connection to a target shard is established. Completes pending handoff promises for that target.
	 */
	void OnConnectionEstablished(const InterLinkIdentifier& targetId);

private:
	void ensureConnectionsAndNotify(
	    std::span<const EntityHandoffRequest> requests,
	    std::span<std::shared_ptr<std::promise<HandoffResult>>> resultPromises);
};
