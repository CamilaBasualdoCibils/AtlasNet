#include "Network/Packet/Packet.hpp"
#pragma

class EntityHandoffCoordinationPacket
	: public TPacket<EntityHandoffCoordinationPacket,
					 "EntityHandoffCoordinationPacket">
{
};
ATLASNET_REGISTER_PACKET(EntityHandoffCoordinationPacket,
						 "EntityHandoffCoordinationPacket");