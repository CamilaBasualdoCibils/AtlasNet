#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/ClusterCommons.hpp"
namespace AtlasNet::Network::Cluster
{
using PacketID = uint64_t;
constexpr static uint32_t PacketMagic_CONST = 'ATLN';
enum class PacketHeaderVersion : uint16_t
{
  V1 = 1,
};

struct PacketPrefix
{
  uint32_t PacketMagic = PacketMagic_CONST;
  PacketHeaderVersion version;
  bool operator==(const PacketPrefix& other) const = default;
  template <typename archive> void serialize(archive& ar)
  {
    ar(PacketMagic, version);
  }
};
struct PacketHeaderV1
{
  PacketPrefix prefix;
  AtlasNetNodeID source;
  AtlasNetNodeID destination;
  uint32_t checksum;
  uint32_t payloadSize;
};

} // namespace AtlasNet::Network::Cluster