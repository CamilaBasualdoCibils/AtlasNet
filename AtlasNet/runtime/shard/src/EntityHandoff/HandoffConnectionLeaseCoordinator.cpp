#include "EntityHandoff/HandoffConnectionLeaseCoordinator.hpp"

#include "InternalDB.hpp"

HandoffConnectionLeaseCoordinator::HandoffConnectionLeaseCoordinator(
	const NetworkIdentity& self, std::shared_ptr<Log> inLogger,
	Options inOptions)
	: selfIdentity(self), logger(std::move(inLogger)),
	  options(std::move(inOptions))
{
}

std::string HandoffConnectionLeaseCoordinator::BuildLeaseKey(
	const NetworkIdentity& peer) const
{
	const std::string self = selfIdentity.ToString();
	const std::string other = peer.ToString();
	if (self < other)
	{
		return options.leaseKeyPrefix + self + "|" + other;
	}
	return options.leaseKeyPrefix + other + "|" + self;
}

bool HandoffConnectionLeaseCoordinator::TryAcquireOrRefreshLease(
	const NetworkIdentity& peer) const
{
	if (!options.leaseEnabled)
	{
		return true;
	}

	const std::string leaseKey = BuildLeaseKey(peer);
	const std::string owner = selfIdentity.ToString();

	const auto existing = InternalDB::Get()->Get(leaseKey);
	if (!existing.has_value())
	{
		const bool setOk = InternalDB::Get()->Set(leaseKey, owner);
		const bool expireOk = InternalDB::Get()->Expire(leaseKey, options.leaseTtl);
		(void)setOk;
		(void)expireOk;
		return true;
	}

	if (existing.value() == owner)
	{
		const bool expireOk = InternalDB::Get()->Expire(leaseKey, options.leaseTtl);
		(void)expireOk;
		return true;
	}

	return false;
}

void HandoffConnectionLeaseCoordinator::ReleaseLeaseIfOwned(
	const NetworkIdentity& peer) const
{
	if (!options.leaseEnabled)
	{
		return;
	}

	const std::string leaseKey = BuildLeaseKey(peer);
	const auto existing = InternalDB::Get()->Get(leaseKey);
	if (existing.has_value() && existing.value() == selfIdentity.ToString())
	{
		const long long removed = InternalDB::Get()->DelKey(leaseKey);
		(void)removed;
	}
}

void HandoffConnectionLeaseCoordinator::MarkConnectionActivity(
	const NetworkIdentity& peer)
{
	lastActivityByPeer[peer] = std::chrono::steady_clock::now();
	if (options.leaseEnabled)
	{
		const bool leaseRefreshed = TryAcquireOrRefreshLease(peer);
		(void)leaseRefreshed;
	}
}

void HandoffConnectionLeaseCoordinator::ReapInactiveConnections(
	std::chrono::steady_clock::time_point now,
	const std::function<void(const NetworkIdentity&, std::chrono::seconds)>&
		onInactive)
{
	for (auto it = lastActivityByPeer.begin(); it != lastActivityByPeer.end();)
	{
		const auto idleFor = now - it->second;
		if (idleFor <= options.inactivityTimeout)
		{
			++it;
			continue;
		}

		ReleaseLeaseIfOwned(it->first);
		if (onInactive)
		{
			onInactive(
				it->first,
				std::chrono::duration_cast<std::chrono::seconds>(idleFor));
		}
		it = lastActivityByPeer.erase(it);
	}
}

void HandoffConnectionLeaseCoordinator::Clear()
{
	lastActivityByPeer.clear();
}
