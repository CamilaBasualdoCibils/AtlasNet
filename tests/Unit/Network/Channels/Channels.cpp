
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelBus.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelTransportProxy.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/V1/ClusterChannelV1.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/IClusterResolver.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "gmock/gmock.h"
#include <gtest/gtest.h>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
using namespace AtlasNet;
using namespace AtlasNet::Network;
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
TEST(Channels, V1BasicTest)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;

  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();

  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));

  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  std::shared_ptr<MockChannelTransport> mockTransport =
      std::make_shared<MockChannelTransport>(nullptr, 0);
  std::shared_ptr<INetworkTransport> networkTransport =
      std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
          Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  EXPECT_CALL(*mockTransport, Send(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly([transport](const AtlasNetNodeID& destination,
                                  std::span<const std::byte> payload)
                      { return transport->Send(destination, payload); });
  EXPECT_CALL(*mockTransport, TryReceive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->TryReceive(packets); });
  EXPECT_CALL(*mockTransport, Receive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->Receive(packets); });
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  Cluster::ClusterChannelV1 channel(options, mockTransport);
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
  channel.Send(thisNodeID,
               std::span<const std::byte>(payload.data(), payload.size()));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::array<Cluster::ClusterMessage, 1> packets;
  size_t received = channel.TryReceive(packets);
  EXPECT_EQ(received, 1);
  EXPECT_EQ(packets[0].Payload().size(), payload.size());
  EXPECT_EQ(
      std::memcmp(packets[0].Payload().data(), payload.data(), payload.size()),
      0);
}

TEST(Channels, V1ReliableReSend_FailedSend)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
  std::shared_ptr<INetworkTransport> networkTransport =
      std::make_shared<Network::UDPNetworkTransport>("NetworkTransport", Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  std::shared_ptr<MockChannelTransport> mockTransport =
      std::make_shared<MockChannelTransport>(nullptr, 0);

  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));

  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  EXPECT_CALL(*mockTransport, Send(testing::_, testing::_))
      .Times(3) // 1. send  fail 2.retry 3. ACK
      .WillOnce(testing::Return(false))
      .WillRepeatedly([transport](const AtlasNetNodeID& destination,
                                  std::span<const std::byte> payload)
                      { return transport->Send(destination, payload); });
  EXPECT_CALL(*mockTransport, TryReceive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->TryReceive(packets); });
  EXPECT_CALL(*mockTransport, Receive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->Receive(packets); });
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  Cluster::ClusterChannelV1 channel(options, mockTransport);
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
  channel.Send(thisNodeID,
               std::span<const std::byte>(payload.data(), payload.size()));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::array<Cluster::ClusterMessage, 10> packets;
  for (int i = 0; i < 30; ++i)
  {
    channel.Tick();
    size_t received = channel.TryReceive(packets);
    if (received > 0)
    {
      EXPECT_EQ(received, 1);
      EXPECT_EQ(packets[0].Payload().size(), payload.size());
      EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload.data(),
                            payload.size()),
                0);
      for (size_t i = 1; i < received; ++i)
      {
        packets[i].Release();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
TEST(Channels, V1ReliableReSend_FailedACK)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
  std::shared_ptr<INetworkTransport> networkTransport =
      std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
          Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  std::shared_ptr<MockChannelTransport> mockTransport =
      std::make_shared<MockChannelTransport>(nullptr, 0);

  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));

  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  EXPECT_CALL(*mockTransport, Send(testing::_, testing::_))
      .Times(4) // 1. send 2.ACK fail 3.retry 4. ACK
      .WillOnce([transport](const AtlasNetNodeID& destination,
                            std::span<const std::byte> payload)
                { return transport->Send(destination, payload); })
      .WillOnce(testing::Return(false))
      .WillRepeatedly([transport](const AtlasNetNodeID& destination,
                                  std::span<const std::byte> payload)
                      { return transport->Send(destination, payload); });
  EXPECT_CALL(*mockTransport, TryReceive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->TryReceive(packets); });
  EXPECT_CALL(*mockTransport, Receive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->Receive(packets); });
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  Cluster::ClusterChannelV1 channel(options, mockTransport);
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
  channel.Send(thisNodeID,
               std::span<const std::byte>(payload.data(), payload.size()));

  std::array<Cluster::ClusterMessage, 10> packets;
  for (int i = 0; i < 30; ++i)
  {
    channel.Tick();
    size_t received = channel.TryReceive(packets);
    if (received > 0)
    {
      EXPECT_EQ(received, 1);
      EXPECT_EQ(packets[0].Payload().size(), payload.size());
      EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload.data(),
                            payload.size()),
                0);
      for (size_t i = 1; i < received; ++i)
      {
        packets[i].Release();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
TEST(Channels, V1Batching)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
      std::shared_ptr<INetworkTransport> networkTransport =
          std::make_shared<Network::UDPNetworkTransport>(
              "NetworkTransport", Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  std::shared_ptr<MockChannelTransport> mockTransport =
      std::make_shared<MockChannelTransport>(nullptr, 0);

  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));

  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  EXPECT_CALL(*mockTransport, Send(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly([transport](const AtlasNetNodeID& destination,
                                  std::span<const std::byte> payload)
                      { return transport->Send(destination, payload); });
  EXPECT_CALL(*mockTransport, TryReceive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->TryReceive(packets); });
  EXPECT_CALL(*mockTransport, Receive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->Receive(packets); });
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Manual,
      .maxBatchBytes = 1024,
  };
  Cluster::ClusterChannelV1 channel(options, mockTransport);
  std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}, std::byte{5}},
                         payload2 = {std::byte{6}, std::byte{7}, std::byte{8},
                                     std::byte{9}, std::byte{10}};
  channel.Send(thisNodeID,
               std::span<const std::byte>(payload1.data(), payload1.size()));
  channel.Send(thisNodeID,
               std::span<const std::byte>(payload2.data(), payload2.size()));
  channel.Flush();
  for (int i = 0; i < 10; ++i)
  {
    std::array<Cluster::ClusterMessage, 10> packets;
    size_t received = channel.TryReceive(packets);
    if (received > 0)
    {
      EXPECT_EQ(received, 2);
      EXPECT_EQ(packets[0].Payload().size(), payload1.size());
      EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload1.data(),
                            payload1.size()),
                0);
      EXPECT_EQ(packets[1].Payload().size(), payload2.size());
      EXPECT_EQ(std::memcmp(packets[1].Payload().data(), payload2.data(),
                            payload2.size()),
                0);

      for (size_t i = 1; i < received; ++i)
      {
        packets[i].Release();
      }
    }
    channel.Tick();
  }

  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
TEST(Channels, V1Sequenced)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
      std::shared_ptr<INetworkTransport> networkTransport =
          std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
              Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  std::shared_ptr<MockChannelTransport> mockTransport =
      std::make_shared<MockChannelTransport>(nullptr, 0);

  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));

  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
  EXPECT_CALL(*mockTransport, Send(testing::_, testing::_))
      .WillRepeatedly([transport](const AtlasNetNodeID& destination,
                                  std::span<const std::byte> payload)
                      { return transport->Send(destination, payload); });
  EXPECT_CALL(*mockTransport, TryReceive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->TryReceive(packets); });
  EXPECT_CALL(*mockTransport, Receive(testing::_))
      .WillRepeatedly([transport](std::span<Cluster::ClusterDatagram> packets)
                      { return transport->Receive(packets); });
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Unreliable,
      .ordering = Cluster::OrderingMode::Sequenced,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  Cluster::ClusterChannelV1 channel(options, mockTransport);
  std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}, std::byte{5}};
  for (int i = 0; i < 10; ++i)
  {
    channel.Send(thisNodeID,
                 std::span<const std::byte>(payload1.data(), payload1.size()));
  }

  channel.Flush();

  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::array<Cluster::ClusterMessage, 10> packets;
  size_t received = channel.Receive(packets);
  EXPECT_EQ(received, 1);
  EXPECT_EQ(packets[0].Payload().size(), payload1.size());
  EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload1.data(),
                        payload1.size()),
            0);

  for (size_t i = 1; i < received; ++i)
  {
    packets[i].Release();
  }
  channel.Flush();
}

/* TEST(Channels, BusSend)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
      std::shared_ptr<INetworkTransport> networkTransport =
          std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
              Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  Cluster::ChannelBus bus({.transport = transport});
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  auto channel = bus.MakeChannel(options);
  std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}, std::byte{5}};
  channel->Send(thisNodeID,
                std::span<const std::byte>(payload1.data(), payload1.size()));
        
} */

TEST(Channels, BusReceive)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));
  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
      std::shared_ptr<INetworkTransport> networkTransport =
          std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
              Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  Cluster::ChannelBus bus({.transport = transport});
  Cluster::ChannelOptions options{
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  };
  auto channel = bus.MakeChannel(options);
  std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}, std::byte{5}};
  channel->Send(thisNodeID,
                std::span<const std::byte>(payload1.data(), payload1.size()));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  bus.TryReceive();
  std::array<Cluster::ClusterMessage, 10> packets;
  size_t received = channel->TryReceive(
      std::span<Cluster::ClusterMessage>(packets.data(), packets.size()));
  EXPECT_EQ(received, 1);
  EXPECT_EQ(packets[0].Payload().size(), payload1.size());
  EXPECT_EQ(std::memcmp(packets[0].Payload().data(), payload1.data(),
                        payload1.size()),
            0);
  for (size_t i = 1; i < received; ++i)
  {
    packets[i].Release();
  }
}
TEST(Channels, BusReceiveMultiChannel)
{
  const AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  const PortType listenPort = 12345;
  std::shared_ptr<MockClusterResolver> resolver =
      std::make_shared<MockClusterResolver>();
  EXPECT_CALL(*resolver, ResolveNodeAddress(testing::_))
      .WillRepeatedly(testing::Return(
          Network::SocketAddress(Network::IPv6::Loopback(), listenPort)));
  EXPECT_CALL(*resolver,
              ResolveNodeID(testing::A<const Network::SocketAddress&>()))
      .WillRepeatedly(testing::Return(thisNodeID));
      std::shared_ptr<INetworkTransport> networkTransport =
          std::make_shared<Network::UDPNetworkTransport>("NetworkTransport",
              Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::shared_ptr<Cluster::ClusterTransport> transport =
      std::make_shared<Cluster::ClusterTransport>(networkTransport, resolver);
  Cluster::ChannelBus bus({.transport = transport});
  auto channel1 = bus.MakeChannel({
      .id = 1,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  });

  auto channel2 = bus.MakeChannel({
      .id = 2,
      .delivery = Cluster::DeliveryMode::Reliable,
      .ordering = Cluster::OrderingMode::Ordered,
      .batching = Cluster::BatchMode::Immediate,
      .maxBatchBytes = 1024,
  });
  std::vector<std::byte> payload1 = {std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}, std::byte{5}};
  std::vector<std::byte> payload2 = {std::byte{6}, std::byte{7}, std::byte{8},
                                     std::byte{9}, std::byte{10}};
  channel1->Send(thisNodeID,
                 std::span<const std::byte>(payload1.data(), payload1.size()));
  channel2->Send(thisNodeID,
                 std::span<const std::byte>(payload2.data(), payload2.size()));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  bus.TryReceive();
  std::array<Cluster::ClusterMessage, 10> packets1;
  size_t received = channel1->TryReceive(
      std::span<Cluster::ClusterMessage>(packets1.data(), packets1.size()));
  EXPECT_EQ(received, 1);
  EXPECT_EQ(packets1[0].Payload().size(), payload1.size());
  EXPECT_EQ(std::memcmp(packets1[0].Payload().data(), payload1.data(),
                        payload1.size()),
            0);
  for (size_t i = 1; i < received; ++i)
  {
    packets1[i].Release();
  }
  size_t received2 = channel2->TryReceive(
      std::span<Cluster::ClusterMessage>(packets1.data(), packets1.size()));
  EXPECT_EQ(received2, 1);
  EXPECT_EQ(packets1[0].Payload().size(), payload2.size());
  EXPECT_EQ(std::memcmp(packets1[0].Payload().data(), payload2.data(),
                        payload2.size()),
            0);
  for (size_t i = 1; i < received2; ++i)
  {
    packets1[i].Release();
  }
}