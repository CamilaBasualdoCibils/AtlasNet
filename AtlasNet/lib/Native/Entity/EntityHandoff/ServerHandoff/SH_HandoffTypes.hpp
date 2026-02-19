#pragma once

#include <cstdint>
#include <string>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"

// Incoming handoff waiting for its transfer tick.
struct SH_PendingIncomingHandoff
{
	AtlasEntity entity;
	NetworkIdentity sender;
	uint64_t transferTick = 0;
};

// Outgoing handoff waiting for commit tick.
struct SH_PendingOutgoingHandoff
{
	AtlasEntityID entityId = 0;
	NetworkIdentity targetIdentity;
	std::string targetClaimKey;
	uint64_t transferTick = 0;
};
