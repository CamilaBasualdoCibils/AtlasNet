#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
namespace AtlasNet::Network
{
class INodeAddressResolver
{
public:
  virtual ~INodeAddressResolver() = default;

  virtual std::optional<SocketAddress> ResolveNodeAddress(AtlasNetNodeID nodeID) = 0;
  virtual std::optional<MACAddress> ResolveNodeMAC(AtlasNetNodeID nodeID) = 0;
};
} // namespace AtlasNet::Network