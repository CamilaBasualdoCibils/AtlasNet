#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
namespace AtlasNet::Network::Cluster
{

struct ClusterDatagram
{
  ClusterDatagram(const TransportDatagram& transportDatagram)
      : transportDatagram(transportDatagram), payload(transportDatagram.payload)
  {
  }
  ClusterDatagram() = default;
  AtlasNetNodeID source;
  std::span<const std::byte> GetPayload() const
  {
    return payload;
  }
  void Release()
  {
    transportDatagram.Release();
  }
  
  private:
  std::span<const std::byte> payload;
  TransportDatagram transportDatagram;
};
} // namespace AtlasNet::Network::Cluster