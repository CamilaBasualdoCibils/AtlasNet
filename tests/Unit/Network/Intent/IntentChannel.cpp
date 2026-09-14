

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelBus.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Intent/ClusterIntentChannel.hpp"
#include "AtlasNet/Core/Network/Intent/IntentRecepient.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "gmock/gmock.h"
#include <gtest/gtest.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
using namespace AtlasNet;
using namespace AtlasNet::Network;
using namespace AtlasNet::Network::Intent;
class MockClusterResolver : public Cluster::IClusterResolver
{
public:
  MOCK_METHOD(std::optional<Network::SocketAddress>, ResolveNodeAddress,
              (AtlasNetNodeID nodeID), (override));
  MOCK_METHOD(std::optional<Network::MACAddress>, ResolveNodeMAC,
              (AtlasNetNodeID nodeID), (override));
  MOCK_METHOD(std::optional<AtlasNetNodeID>, ResolveNodeID,
              (const Network::SocketAddress& address), (override));
  MOCK_METHOD(std::optional<AtlasNetNodeID>, ResolveNodeID,
              (const Network::MACAddress& mac), (override));
};
class MockChannelTransport : public Cluster::ChannelTransportProxy
{
public:
  MockChannelTransport(Cluster::ChannelBus* channelBus,
                       Cluster::ChannelID channelID)
      : Cluster::ChannelTransportProxy(channelBus, channelID)
  {
  }
  MOCK_METHOD(bool, Send,
              (const AtlasNetNodeID& destination,
               std::span<const std::byte> payload),
              (override));
  MOCK_METHOD(size_t, Receive, (std::span<Cluster::ClusterDatagram> packets),
              (override));
  MOCK_METHOD(size_t, TryReceive, (std::span<Cluster::ClusterDatagram> packets),
              (override));
};
class MockIntentResolver : public IIntentResolver
{
public:
  MOCK_METHOD(std::optional<AtlasNetNodeID>, ResolveIntent,
              (const VIntent& intent), (override));
};
TEST(IntentChannel, Self)
{
  spdlog::set_level(spdlog::level::trace);
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> clusterResolver =
      std::make_shared<MockClusterResolver>();
  EXPECT_CALL(*clusterResolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));
  EXPECT_CALL(*clusterResolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  std::shared_ptr<INetworkTransport> baseTransport =
      std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(baseTransport,
                                                     clusterResolver);
  Cluster::ChannelBus bus({.transport = transport});
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  std::shared_ptr<MockIntentResolver> intentResolver =
      std::make_shared<MockIntentResolver>();
  EXPECT_CALL(*intentResolver, ResolveIntent(testing::_))
      .WillRepeatedly(testing::Return(thisNodeID));
  Intent::ClusterIntentChannel channel(thisNodeID, bus.MakeChannel(options),
                                       intentResolver);
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
  channel.Send(
      Recepient::ShardOfEntityRecepient{.entityID = AtlasNetEntityID(42)},
      payload, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::array<Intent::IntentDatagram, 1> packets;
  bool received = false;
  for (int i = 0; i < 15; ++i)
  {
    bus.TryReceive();
    channel.Tick();
    size_t newlyReceived = channel.TryReceive(packets);
    spdlog::trace("Attempt {}: received {} packets", i + 1, newlyReceived);
    if (newlyReceived > 0)
    {
      EXPECT_EQ(newlyReceived, 1);
      EXPECT_EQ(packets[0].Payload().size(), payload.size());
      EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload.data(),
                            payload.size()),
                0);
      channel.Tick();
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_TRUE(received) << "Failed to receive the message after 15 attempts";
}
TEST(IntentChannel, Redirect)
{
  /* spdlog::set_level(spdlog::level::trace);
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> clusterResolver =
      std::make_shared<MockClusterResolver>();
  EXPECT_CALL(*clusterResolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));
EXPECT_CALL(*clusterResolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  std::shared_ptr<Cluster::IClusterTransport> transport =
      std::make_shared<Cluster::UDPClusterTransport>(listenPort,
                                                     clusterResolver);
  Cluster::ChannelBus bus({.transport = transport});
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  std::shared_ptr<MockIntentResolver> intentResolver =
      std::make_shared<MockIntentResolver>();
  EXPECT_CALL(*intentResolver, ResolveIntent(testing::_))
      .WillRepeatedly(testing::Return(thisNodeID));
  Intent::ClusterIntentChannel channel(thisNodeID, bus.MakeChannel(options),
                                       intentResolver);
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
  channel.Send(
      Recepient::ShardOfEntityRecepient{.entityID = AtlasNetEntityID(42)},
      payload, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::array<Intent::IntentDatagram, 1> packets;
  bool received = false;
  for (int i = 0; i < 15; ++i)
  {
    bus.TryReceive();
    channel.Tick();
    size_t newlyReceived = channel.TryReceive(packets);
    spdlog::trace("Attempt {}: received {} packets", i + 1, newlyReceived);
    if (newlyReceived > 0)
    {
      EXPECT_EQ(newlyReceived, 1);
      EXPECT_EQ(packets[0].Payload().size(), payload.size());
      EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload.data(),
                            payload.size()),
                0);
      channel.Tick();
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_TRUE(received) << "Failed to receive the message after 15 attempts"; */
}