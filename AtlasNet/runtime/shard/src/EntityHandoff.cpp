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

void EntityHandoff::ensureConnectionsAndNotify(std::span<const EntityHandoffRequest> requests)
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

	// Notify per-entity result. Success if we have the connection (we own it or other shard initiates).
	if (!resultCallback_)
		return;
	for (const auto& r : requests)
	{
		if (r.targetPartitionKey.empty())
			continue;
		const std::string& target = r.targetPartitionKey;
		bool success = (openConnectionsByTarget_.count(target) != 0) ||
		               !shouldInitiateConnectionTo(self, target);
		resultCallback_(r.entity.Entity_ID, success);
	}
}

void EntityHandoff::RequestHandoff(std::span<const EntityHandoffRequest> requests)
{
	ensureConnectionsAndNotify(requests);
}
