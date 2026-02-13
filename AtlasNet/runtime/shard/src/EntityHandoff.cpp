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

void EntityHandoff::CompletePendingSuccessPromises()
{
	for (auto& [prom, entityId] : pendingSuccessPromises_)
	{
		if (prom)
			prom->set_value(HandoffResult{ entityId, true });
	}
	pendingSuccessPromises_.clear();
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

	// Ensure connection for each target we're responsible for; update openConnectionsByTarget_.
	for (const auto& [target, entityIds] : currentByTarget)
	{
		if (entityIds.empty())
			continue;
		bool alreadyHad = (openConnectionsByTarget_.find(target) != openConnectionsByTarget_.end());
		bool connected = alreadyHad;
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
						logger->DebugFormatted("EntityHandoff: opened connection to {} (entity out of bounds)",
						                      target);
						connected = true;
					}
					else
						logger->WarningFormatted("EntityHandoff: failed to open connection to {}", target);
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
		if (connected)
			openConnectionsByTarget_[target] = entityIds;
	}

	// Complete each request's future: failure immediately, success stubbed (completed at end via CompletePendingSuccessPromises).
	pendingSuccessPromises_.clear();
	const size_t n = std::min(requests.size(), resultPromises.size());
	for (size_t i = 0; i < n; ++i)
	{
		const auto& r = requests[i];
		auto& prom = resultPromises[i];
		if (!prom || r.targetPartitionKey.empty())
			continue;
		const std::string& target = r.targetPartitionKey;
		bool success = (openConnectionsByTarget_.count(target) != 0) ||
		               !shouldInitiateConnectionTo(self, target);
		if (!success)
			prom->set_value(HandoffResult{ r.entity.Entity_ID, false });
		else
			pendingSuccessPromises_.emplace_back(prom, r.entity.Entity_ID);
	}

	// Stub: complete success futures now; in production, complete when connection reaches eConnected.
	CompletePendingSuccessPromises();
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
