#pragma once
#include "Packet.hpp"

#include "InterlinkIdentifier.hpp"
class RelayPacket : IPacket
{

    std::shared_ptr<IPacket> sub_Packet;
    

};