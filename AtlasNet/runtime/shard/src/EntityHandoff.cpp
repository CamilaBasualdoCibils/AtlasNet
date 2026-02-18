#include "EntityHandoff.hpp"
#include "DockerIO.hpp"
#include "Database/ServerRegistry.hpp"
#include "Interlink.hpp"

std::string EntityHandoff::getSelfPartitionKey() const
{
	return DockerIO::Get().GetSelfContainerName();
}

InterLinkIdentifier EntityHandoff::partitionKeyToIdentifier(const std::string& partitionKey)
{
	auto parsed = InterLinkIdentifier::FromString(partitionKey);
	if (parsed.has_value())
		return *parsed;
	return InterLinkIdentifier::MakeIDShard(partitionKey);
}

bool EntityHandoff::shouldInitiateConnectionTo(const std::string& selfPartitionKey,
                                               const std::string& targetPartitionKey)
{
	auto targetParsed = InterLinkIdentifier::FromString(targetPartitionKey);
	std::string targetId =
	    targetParsed.has_value() ? std::string(targetParsed->ID.data()) : targetPartitionKey;
	return selfPartitionKey < targetId;
}

void EntityHandoff::CompletePromisesForTarget(const std::string& targetPartitionKey)
{
	auto it = pendingPromisesByTarget_.find(targetPartitionKey);
	if (it == pendingPromisesByTarget_.end())
		return;

	for (auto& [prom, entityId] : it->second)
	{
		if (prom)
			prom->set_value(HandoffResult{ entityId, true });
	}
	pendingPromisesByTarget_.erase(it);
}

void EntityHandoff::OnConnectionEstablished(const InterLinkIdentifier& targetId)
{
	// Find the partition key that corresponds to this targetId by checking pending promises
	// (we might not have added it to openConnectionsByTarget_ yet)
	std::string targetPartitionKey;
	for (const auto& [partitionKey, _] : pendingPromisesByTarget_)
	{
		InterLinkIdentifier parsed = partitionKeyToIdentifier(partitionKey);
		if (parsed.ToString() == targetId.ToString() || 
		    (parsed.Type == targetId.Type && std::string(parsed.ID.data()) == std::string(targetId.ID.data())))
		{
			targetPartitionKey = partitionKey;
			break;
		}
	}

	if (!targetPartitionKey.empty())
	{
		logger->DebugFormatted("EntityHandoff: connection established to {}, completing {} promises",
		                     targetPartitionKey, pendingPromisesByTarget_[targetPartitionKey].size());
		CompletePromisesForTarget(targetPartitionKey);
		// Mark connection as open now that it's established
		// Note: entityIds will be updated on next TickHandoff when entities are still OOB
		if (openConnectionsByTarget_.find(targetPartitionKey) == openConnectionsByTarget_.end())
			openConnectionsByTarget_[targetPartitionKey] = {};
	}
}

void EntityHandoff::ensureConnectionsAndNotify(
    std::span<const EntityHandoffRequest> requests,
    std::span<std::shared_ptr<std::promise<HandoffResult>>> resultPromises)
{
	const std::string self = getSelfPartitionKey();
	std::unordered_map<std::string, std::unordered_set<EntityID>> currentByTarget;

	for (const auto& r : requests)
	{
		if (r.targetPartitionKey.empty())
			continue;
		auto parsed = InterLinkIdentifier::FromString(r.targetPartitionKey);
		if (parsed.has_value() && std::string(parsed->ID.data()) == self)
			continue;
		currentByTarget[r.targetPartitionKey].insert(r.entity.Entity_ID);
	}

	// Close connections for targets that no longer have any entity.
	for (auto it = openConnectionsByTarget_.begin(); it != openConnectionsByTarget_.end();)
	{
		const std::string& target = it->first;
		auto curIt = currentByTarget.find(target);
		if (curIt == currentByTarget.end() || curIt->second.empty())
		{
			InterLinkIdentifier targetId = partitionKeyToIdentifier(target);
			Interlink::Get().CloseConnectionTo(targetId, 0, "entity back in bounds");
			logger->DebugFormatted("EntityHandoff: closed connection to {} (entity back in bounds)", target);
			it = openConnectionsByTarget_.erase(it);
		}
		else
			++it;
	}

	// Ensure connection for each target we're responsible for; update openConnectionsByTarget_ when actually connected.
	for (const auto& [target, entityIds] : currentByTarget)
	{
		if (entityIds.empty())
			continue;
		bool alreadyHad = (openConnectionsByTarget_.find(target) != openConnectionsByTarget_.end());
		if (!alreadyHad && shouldInitiateConnectionTo(self, target))
		{
			InterLinkIdentifier targetId = partitionKeyToIdentifier(target);
			if (!ServerRegistry::Get().ExistsInRegistry(targetId))
			{
				logger->DebugFormatted("EntityHandoff: target {} not in registry yet, skipping connect", target);
			}
			else
			{
				try
				{
					if (Interlink::Get().EstablishConnectionTo(targetId))
					{
						logger->DebugFormatted("EntityHandoff: queued connection to {} (entity out of bounds)",
						                      target);
						// Don't add to openConnectionsByTarget_ yet - wait for OnConnectionEstablished
					}
					else
						logger->WarningFormatted("EntityHandoff: failed to queue connection to {}", target);
				}
				catch (const std::bad_optional_access&)
				{
					logger->WarningFormatted("EntityHandoff: target {} not in registry (lookup failed), skipping",
					                        target);
				}
				catch (const std::exception& e)
				{
					logger->WarningFormatted("EntityHandoff: failed to connect to {}: {}", target, e.what());
				}
			}
		}
		// If already had connection, keep it; otherwise it will be added in OnConnectionEstablished
		if (alreadyHad)
			openConnectionsByTarget_[target] = entityIds;
	}

	// Complete each request's future: failure immediately, success when connection is established.
	const size_t n = std::min(requests.size(), resultPromises.size());
	for (size_t i = 0; i < n; ++i)
	{
		const auto& r = requests[i];
		auto& prom = resultPromises[i];
		if (!prom || r.targetPartitionKey.empty())
			continue;
		const std::string& target = r.targetPartitionKey;
		bool alreadyConnected = (openConnectionsByTarget_.count(target) != 0);
		bool weAreInitiator = shouldInitiateConnectionTo(self, target);
		
		if (!alreadyConnected && weAreInitiator)
		{
			// Connection is being established asynchronously; store promise to complete when OnConnectionEstablished fires.
			pendingPromisesByTarget_[target].emplace_back(prom, r.entity.Entity_ID);
		}
		else if (alreadyConnected || !weAreInitiator)
		{
			// Already connected or other shard initiates; complete immediately.
			prom->set_value(HandoffResult{ r.entity.Entity_ID, true });
		}
		else
		{
			// Connection failed; complete with failure.
			prom->set_value(HandoffResult{ r.entity.Entity_ID, false });
		}
	}
}

std::vector<std::future<HandoffResult>> EntityHandoff::RequestHandoff(std::span<const EntityHandoffRequest> requests)
{
	const size_t n = requests.size();
	std::vector<std::shared_ptr<std::promise<HandoffResult>>> promises(n);
	std::vector<std::future<HandoffResult>> futures;
	futures.reserve(n);
	for (size_t i = 0; i < n; ++i)
	{
		promises[i] = std::make_shared<std::promise<HandoffResult>>();
		futures.push_back(promises[i]->get_future());
	}
	ensureConnectionsAndNotify(requests, promises);
	return futures;
}
