#pragma once

#include "boost/describe/enum.hpp"
#include <cstdint>
#include <boost/describe.hpp>
namespace AtlasNet::Network
{
using PacketID = uint64_t;
enum class PacketSendMode : uint8_t
{
  /*Send immediately or drop*/
  NoDelay = 0,

  /*Send unreliably, No ACK*/
  Unreliable = 1,
  UnreliableBatched = 2,

  /*Send reliably, Requires ACK*/
  Reliable = 3,
  ReliableBatched = 4,

  INVALID = 5
};
BOOST_DESCRIBE_ENUM(PacketSendMode, NoDelay, Unreliable, UnreliableBatched, Reliable, ReliableBatched, INVALID)
enum class DatagramTransportType : uint8_t
{
  INVALID = 0,
  UDP = 1,
  TCP = 2,
  WebSocket = 3,
  SteamNetSock = 4,
  DPDK = 5,
};
BOOST_DESCRIBE_ENUM(DatagramTransportType, UDP, TCP, WebSocket, SteamNetSock, DPDK, INVALID)
enum class TransportSide : uint8_t
{
  INVALID = 0,
  Ingress = 1,
  Internal = 2,
};
BOOST_DESCRIBE_ENUM(TransportSide, Ingress, Internal, INVALID)

} // namespace AtlasNet::Network