#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Packet/NetPacket.hpp"
#include "AtlasNet/Core/Network/NodeAddressResolver.hpp"
#include "AtlasNet/Core/Network/Transport/Datagram/DatagramCommons.hpp"
#include "AtlasNet/Core/Network/Transport/DatagramBuffer.hpp"
#include "AtlasNet/Core/Network/Transport/ITransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportCommons.hpp"
#include <cstdint>
#include <span>
#include <vector>
namespace AtlasNet::Network
{

class IClusterTransport : public ITransport
{
public:
  IClusterTransport(std::shared_ptr<INodeAddressResolver> resolver)
      : nodeResolver(std::move(resolver))
  {
  }
  virtual ~IClusterTransport() = default;

  virtual void SendMessage(const AtlasNetNodeID& node,
                           const PacketPayloadView& data) = 0;
  virtual bool Listen() = 0;
  virtual size_t Receive(std::span<DatagramBuffer> packets) = 0;

  virtual size_t TryReceive(std::span<DatagramBuffer> packets) = 0;

  INodeAddressResolver& GetAddressResolver() const
  {
    return *nodeResolver.get();
  }

private:
  std::shared_ptr<INodeAddressResolver> nodeResolver;
};
} // namespace AtlasNet::Network