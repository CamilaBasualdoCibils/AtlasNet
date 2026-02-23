#pragma once

#include "Entity/Entity.hpp"
#include "Entity/EntityEnums.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/NetworkIdentity.hpp"
using TransferID = UUID;
struct EntityTransferData
{
	TransferID ID;
	NetworkIdentity receiver;
	EntityTransferStage stage = EntityTransferStage::eNone;
	boost::container::small_vector<AtlasEntityID, 32> entityIDs;
	bool WaitingOnResponse = false;
};
