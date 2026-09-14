#pragma once

#include "AtlasNet/Core/Network/Topology/ITopologyAssigner.hpp"
namespace AtlasNet::Network::Topology
{
    class RPCAssigner : public ITopologyAssigner
    {
        public:
            virtual ~RPCAssigner() = default;
    };
}