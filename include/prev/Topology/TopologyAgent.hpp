#pragma once

#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyAssigner.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionTransport.hpp"
namespace AtlasNet::Network::Topology
{

class TopologyAgent
{

public:
  TopologyAgent(std::shared_ptr<AtlasNet::Network::IConnectionTransport> transport,
                std::unique_ptr<ITopologyAssigner> _assigner)
      : transport(transport), assigner(std::move(_assigner))
  {
    assigner->OnConnectionAssign([this](AtlasNetNodeID a, AtlasNetNodeID b)
                                 { OnConnectionAssign(a, b); });
    assigner->OnDisconnectionAssign([this](AtlasNetNodeID a, AtlasNetNodeID b)
                                    { OnDisconnectionAssign(a, b); });
  }

private:
  void OnConnectionAssign(AtlasNetNodeID a, AtlasNetNodeID b) {}
  void OnDisconnectionAssign(AtlasNetNodeID a, AtlasNetNodeID b) {}
  std::shared_ptr<AtlasNet::Network::IConnectionTransport> transport;
  std::unique_ptr<ITopologyAssigner> assigner;
};
} // namespace AtlasNet::Network::Topology