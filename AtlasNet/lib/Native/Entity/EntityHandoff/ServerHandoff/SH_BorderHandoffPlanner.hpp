#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SH_HandoffTypes.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;
class SH_EntityAuthorityTracker;

// Finds entities that crossed local bounds.
// Sends timed handoff packets and returns pending outgoing records.
class SH_BorderHandoffPlanner
{
  public:
	struct Options
	{
		// How many ticks ahead to schedule the switch.
		uint64_t handoffLeadTicks = 6;
	};

	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger);
	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger,
							Options inOptions);

	// Plans/sends outgoing handoffs for this tick.
	// Returns pending outgoing records for mailbox tracking.
	std::vector<SH_PendingOutgoingHandoff> PlanAndSendAll(
		SH_EntityAuthorityTracker& tracker,
		uint64_t localAuthorityTick) const;

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
};
