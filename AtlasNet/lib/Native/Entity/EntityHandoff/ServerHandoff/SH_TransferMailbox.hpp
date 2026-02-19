#pragma once

#include <memory>
#include <optional>

#include "SH_HandoffTypes.hpp"

class DebugEntityOrbitSimulator;
class Log;
class NH_EntityAuthorityTracker;
class SH_TelemetryPublisher;

class SH_TransferMailbox
{
  public:
	explicit SH_TransferMailbox(std::shared_ptr<Log> inLogger);

	void Reset();
	void QueueIncoming(const AtlasEntity& entity, const NetworkIdentity& sender,
					   uint64_t transferTick);
	bool AdoptIncomingIfDue(uint64_t localAuthorityTick,
							DebugEntityOrbitSimulator& debugSimulator);
	void SetPendingOutgoing(const SH_PendingOutgoingHandoff& handoff);
	bool CommitOutgoingIfDue(uint64_t localAuthorityTick,
							 DebugEntityOrbitSimulator& debugSimulator,
							 NH_EntityAuthorityTracker& tracker,
							 const SH_TelemetryPublisher& telemetryPublisher);
	void ClearPendingOutgoing();

	[[nodiscard]] bool HasPendingOutgoing() const
	{
		return pendingOutgoing.has_value();
	}

  private:
	std::shared_ptr<Log> logger;
	std::optional<SH_PendingIncomingHandoff> pendingIncoming;
	std::optional<SH_PendingOutgoingHandoff> pendingOutgoing;
};
