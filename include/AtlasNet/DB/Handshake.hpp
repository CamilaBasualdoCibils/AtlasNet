#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
namespace AtlasNet::DB
{

using RPC_DB_Ping =
    Network::RPC::RPCMethod<"AtlasNet.DB.Ping", int,int>;

struct RegisterNodeRequest
{
    AtlasNetNodeID nodeID;
    Network::SocketAddress handshakeAddress,channelBusAddress;
    Network::MACAddress macAddress;
    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar(nodeID, handshakeAddress, channelBusAddress, macAddress);
    }
};
enum class RegisterNodeResponse
{
    SUCCESS,
    FAILURE,
};
/* struct RegisterNodeResponse
{
    template <typename Archive>
    void serialize(Archive& ar)
    {

    }
}; */
using RPC_DB_RegisterNode =
    Network::RPC::RPCMethod<"AtlasNet.DB.RegisterNode", RegisterNodeResponse,
                            RegisterNodeRequest>;
} // namespace AtlasNet::DB