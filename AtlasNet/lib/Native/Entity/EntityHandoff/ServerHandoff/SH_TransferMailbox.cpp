// SH transfer mailbox.
// Stores pending incoming/outgoing handoffs and applies transfer-tick actions.

#include "SH_TransferMailbox.hpp"

#include <utility>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityTracker.hpp"
#include "SH_TelemetryPublisher.hpp"

SH_TransferMailbox::SH_TransferMailbox(std::shared_ptr<Log> inLogger)
	: logger(std::move(inLogger))
{
}

void SH_TransferMailbox::Reset()
{
	pendingIncoming.reset();
	pendingOutgoing.reset();
}

void SH_TransferMailbox::QueueIncoming(const AtlasEntity& entity,
									 const NetworkIdentity& sender,
									 const uint64_t transferTick)
{
	pendingIncoming = SH_PendingIncomingHandoff{
		.entity = entity, .sender = sender, .transferTick = transferTick};

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff][SH] Received handoff entity={} from={} transfer_tick={}",
			entity.Entity_ID, sender.ToString(), transferTick);
	}
}

bool SH_TransferMailbox::AdoptIncomingIfDue(
	const uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator)
{
	if (!pendingIncoming.has_value() ||
		localAuthorityTick < pendingIncoming->transferTick)
	{
		return false;
	}

	debugSimulator.AdoptSingleEntity(pendingIncoming->entity);
	pendingIncoming.reset();
	return true;
}

void SH_TransferMailbox::SetPendingOutgoing(
	const SH_PendingOutgoingHandoff& handoff)
{
	pendingOutgoing = handoff;
}

bool SH_TransferMailbox::CommitOutgoingIfDue(
	const uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator,
	NH_EntityAuthorityTracker& tracker,
	const SH_TelemetryPublisher& telemetryPublisher)
{
	if (!pendingOutgoing.has_value() ||
		localAuthorityTick < pendingOutgoing->transferTick)
	{
		return false;
	}

	debugSimulator.Reset();
	tracker.SetOwnedEntities({});
	telemetryPublisher.PublishFromTracker(tracker);

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff][SH] Committed outgoing handoff entity={} target={} "
			"transfer_tick={}",
			pendingOutgoing->entityId, pendingOutgoing->targetIdentity.ToString(),
			pendingOutgoing->transferTick);
	}

	pendingOutgoing.reset();
	return true;
}

void SH_TransferMailbox::ClearPendingOutgoing()
{
	pendingOutgoing.reset();
}
