#pragma once

#include <cstdint>
#include <string>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"

struct SH_PendingIncomingHandoff
{
	AtlasEntity entity;
	NetworkIdentity sender;
	uint64_t transferTick = 0;
};

struct SH_PendingOutgoingHandoff
{
	AtlasEntityID entityId = 0;
	NetworkIdentity targetIdentity;
	std::string targetClaimKey;
	uint64_t transferTick = 0;
};
