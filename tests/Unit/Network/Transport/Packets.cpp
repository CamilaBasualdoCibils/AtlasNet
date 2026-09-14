
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <gtest/gtest.h>

/* TEST(Packets, Basic)
{
  using namespace AtlasNet::Network;
  PacketView packet;
  {
    V1::DatagramHeader header;
    header.destinationNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.sourceNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.messageID = 42;
    packet.SetHeader(header);
  }

  PacketPayload payload = {1, 2, 3, 4, 5};
  packet.SetPayload(payload);
  packet.Finalize();

  ASSERT_TRUE(packet.Validate());

  PacketView receivedPacket;
  receivedPacket.SetHeader(packet.GetHeader());
  receivedPacket.SetPayload(packet.GetPayload());

  ASSERT_TRUE(receivedPacket.Validate());
}
TEST(Packets, Invalid)
{
  using namespace AtlasNet::Network;
  PacketView packet;
  {
    V1::DatagramHeader header;
    header.destinationNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.sourceNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.messageID = 42;
    packet.SetHeader(header);
  }

  PacketPayload payload = {1, 2, 3, 4, 5};
  packet.SetPayload(payload);

  ASSERT_FALSE(packet.Validate());
}
TEST(Packets, EmptyPayload)
{
  using namespace AtlasNet::Network;
  PacketView packet;
  {
    V1::DatagramHeader header;
    header.destinationNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.sourceNodeID = AtlasNet::AtlasNetNodeID::Generate();
    header.messageID = 42;
    packet.SetHeader(header);
  }

  PacketPayload payload = {};
  packet.SetPayload(payload);
  packet.Finalize();

  ASSERT_TRUE(packet.Validate());
}
 */
/* TEST(Packets, Serialization)
{
  using namespace AtlasNet::Network;

  V1::DatagramHeader header;
  header.protocol = V1::DatagramProtocol::Raw;
  header.payloadSize = 5;



  AtlasNet::NetBinaryWriter writer;
  writer(header);

  auto data = writer.GetBytes();

  AtlasNet::NetBinaryReader reader(data.data(), data.size());
  V1::DatagramHeader receivedHeader;
  reader(receivedHeader);

  EXPECT_EQ(receivedHeader,header);
} */