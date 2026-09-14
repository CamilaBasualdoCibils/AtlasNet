#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterDatagram.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/IClusterResolver.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <stdexcept>

namespace AtlasNet::Network::Cluster
{
class ClusterTransport
{
public:
  ClusterTransport(std::shared_ptr<INetworkTransport> transport,
                   std::shared_ptr<IClusterResolver> resolver)
      : transport(std::move(transport)), nodeResolver(std::move(resolver))
  {
  }
  virtual ~ClusterTransport() = default;
  bool Send(const AtlasNetNodeID& destination,
            std::span<const std::byte> payload)
  {
    std::optional<SocketAddress> resolvedDestination =
        GetResolver().ResolveNodeAddress(destination);
    if (!resolvedDestination.has_value())
      throw std::runtime_error("Failed to resolve destination address");
    return transport->Send(resolvedDestination.value(), payload);
  }

  [[nodiscard]] size_t Receive(std::span<ClusterDatagram> packets)
  {
    std::array<TransportDatagram, 64> transportBuffer;
    size_t received = transport->Receive(transportBuffer);

    size_t outputPacket = 0;
    for (size_t i = 0; i < received && i < packets.size(); ++i)
    {
      TransportDatagram& transportDatagram = transportBuffer[i];
      ClusterDatagram datagram(transportDatagram);
      const std::optional<AtlasNetNodeID> sourceID =
          GetResolver().ResolveNodeID(transportDatagram.source);
      if (!sourceID)
      {
        logger->warn("Failed to resolve source address: {}, packet dropped",
                     transportDatagram.source.to_string());
        continue;
      }
      packets[outputPacket++] = datagram;
    }
    return outputPacket;
  }

  [[nodiscard]] size_t TryReceive(std::span<ClusterDatagram> packets)
  {
    std::array<TransportDatagram, 64> transportBuffer;
    size_t received = transport->TryReceive(transportBuffer);

    size_t outputPacket = 0;
    for (size_t i = 0; i < received && i < packets.size(); ++i)
    {
      TransportDatagram& transportDatagram = transportBuffer[i];
      ClusterDatagram datagram(transportDatagram);
      const std::optional<AtlasNetNodeID> sourceID =
          GetResolver().ResolveNodeID(transportDatagram.source);
      if (!sourceID)
      {
        logger->warn("Failed to resolve source address: {}, packet dropped",
                     transportDatagram.source.to_string());
        continue;
      }
      packets[outputPacket++] = datagram;
    }
    return outputPacket;
  }

protected:
  IClusterResolver& GetResolver() const
  {
    return *nodeResolver;
  }

private:
  std::shared_ptr<INetworkTransport> transport;
  std::shared_ptr<IClusterResolver> nodeResolver;
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("ClusterTransport");
};
} // namespace AtlasNet::Network::Cluster