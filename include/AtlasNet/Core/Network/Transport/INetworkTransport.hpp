#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
namespace AtlasNet::Network
{
class INetworkTransport
{
public:
  virtual ~INetworkTransport() = default;
  INetworkTransport() = default;
  virtual bool Send(const SocketAddress& destination,
            std::span<const std::byte> payload) = 0;

  virtual size_t Receive(std::span<TransportDatagram> packets) = 0;

  virtual size_t TryReceive(std::span<TransportDatagram> packets) = 0;

  virtual PortType GetListenPort() = 0;
  virtual SocketAddress GetListenAddress() = 0;
};
} // namespace AtlasNet::Network