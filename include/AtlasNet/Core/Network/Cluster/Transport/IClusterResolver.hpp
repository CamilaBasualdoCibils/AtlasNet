#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
namespace AtlasNet::Network::Cluster
{
    class IClusterResolver
    {
    public:
        virtual ~IClusterResolver() = default;
        virtual std::optional<SocketAddress> ResolveNodeAddress(AtlasNetNodeID nodeID) = 0;
        virtual std::optional<MACAddress> ResolveNodeMAC(AtlasNetNodeID nodeID) = 0;
        virtual std::optional<AtlasNetNodeID> ResolveNodeID(const SocketAddress& address) = 0;
        virtual std::optional<AtlasNetNodeID> ResolveNodeID(const MACAddress& mac) = 0;
    };
}