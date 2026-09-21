#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include "AtlasNet/Core/Service/AtlasNetService.hpp"
namespace AtlasNet::DB::RPC
{

using RPC_DB_Ping = Network::RPC::RPCMethod<"AtlasNet.DB.Ping", int, int>;

struct RegisterNodeRequest
{
  AtlasNetNodeID nodeID;
  Network::SocketAddress handshakeAddress, channelBusAddress;
  Network::MACAddress macAddress;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(nodeID, handshakeAddress, channelBusAddress, macAddress);
  }
};
enum class RegisterNodeResponse
{
  SUCCESS,
  FAILURE,
};

using RPC_DB_RegisterNode =
    Network::RPC::RPCMethod<"AtlasNet.DB.RegisterNode", RegisterNodeResponse,
                            RegisterNodeRequest>;

struct RegisteredNode
{
    AtlasNetServiceType serviceType = AtlasNetServiceType::INVALID;
  AtlasNetNodeID nodeID;
  Network::SocketAddress handshakeAddress, channelBusAddress;
  Network::MACAddress macAddress;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(nodeID, handshakeAddress, channelBusAddress, macAddress);
  }
};
using RPC_DB_GetNodes =
    Network::RPC::RPCMethod<"AtlasNet.DB.GetNodes", std::vector<RegisteredNode>,
                            void>;

} // namespace AtlasNet::DB::RPC