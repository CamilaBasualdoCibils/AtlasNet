#include "Packet.hpp"


IPacket& IPacket::Deserialize(ByteReader& br) 
{
    packet_type = br.read_scalar<decltype(packet_type)>();
    DeserializeData(br);
}
IPacket& IPacket::Serialize(ByteWriter& bw) const 
{
    bw.write_scalar<decltype(packet_type)>(packet_type);
    SerializeData(bw);
}
