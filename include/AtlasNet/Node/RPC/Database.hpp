#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include "AtlasNet/Node/NodeCapability.hpp"
#include <vector>

// RPC contracts offered by nodes with NodeCapability::Database.
// Callers use these declarations regardless of how the destination node is
// hosted.

namespace AtlasNet::RPC::Database
{
struct StartupInfo
{
  AtlasNetNodeID nodeID;
  Network::SocketAddress channelBusAddress;

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(nodeID, channelBusAddress);
  }
};
using Ping = Network::RPC::RPCMethod<"AtlasNet.DB.Ping", StartupInfo, int>;

struct RegisterNodeRequest
{
  AtlasNetNodeID nodeID;
  Network::SocketAddress handshakeAddress, channelBusAddress;
  Network::MACAddress macAddress;
  NodeCapability capabilities = DefaultNodeCapabilities;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(nodeID, handshakeAddress, channelBusAddress, macAddress, capabilities);
  }
};

enum class RegisterNodeResponse
{
  SUCCESS,
  FAILURE
};
using RegisterNode =
    Network::RPC::RPCMethod<"AtlasNet.DB.RegisterNode", RegisterNodeResponse,
                            RegisterNodeRequest>;

enum class ClaimControllerPromotionResponse
{
  CLAIMED,
  ALREADY_CLAIMED,
  FAILURE
};
using ClaimControllerPromotion = Network::RPC::RPCMethod<
    "AtlasNet.DB.ClaimControllerPromotion", ClaimControllerPromotionResponse,
    AtlasNetNodeID>;

// Declared for registry discovery; a handler is not implemented yet.
using RegisteredNode = RegisterNodeRequest;
using GetNodes = Network::RPC::RPCMethod<"AtlasNet.DB.GetNodes",
                                         std::vector<RegisteredNode>, void>;

} // namespace AtlasNet::RPC::Database
