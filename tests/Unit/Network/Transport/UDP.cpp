
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "Commons.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
using namespace AtlasNet;

TEST(UDP, Init)
{
 
  Network::UDPNetworkTransport transport("NetworkTransport",
      Network::SocketAddress(Network::IPv6::Any(), 12345));
}

TEST(UDP, SendReceive)
{
  AtlasNetNodeID thisNodeID = AtlasNetNodeID::Generate();
  Network::PortType listenPort = pick_available_port();
  Network::UDPNetworkTransport transport("NetworkTransport",
      Network::SocketAddress(Network::IPv6::Any(), listenPort));
  std::vector<std::byte> payload = {std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};

  transport.Send(Network::SocketAddress(Network::IPv6::Loopback(), listenPort), payload);

  std::array<Network::TransportDatagram, 1> packets;
  size_t received = transport.Receive(packets);
  ASSERT_EQ(received, 1);
  ASSERT_EQ(packets[0].payload.size(), payload.size());
  ASSERT_EQ(
      std::memcmp(packets[0].payload.data(), payload.data(), payload.size()),
      0);
  for (const auto& datagram : packets)
  {
    spdlog::log(spdlog::level::info, "Received datagram from {} with {} bytes",
                datagram.source.to_string(), datagram.payload.size());
  }
  size_t receivedAgain = transport.TryReceive(packets);
  ASSERT_EQ(receivedAgain, 0);
  packets[0].Release();
}
