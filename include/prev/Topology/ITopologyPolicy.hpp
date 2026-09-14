#pragma once

#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
namespace AtlasNet::Network::Topology
{
class ITopologyPolicy
{
public:
  virtual ~ITopologyPolicy() = default;

  WeightGraph ApplyPolicy(const std::vector<NetworkNodeInfo>& nodes)
  {
    WeightGraph weightGraph;

    for (const auto& node : nodes)
    {
      weightGraph.AddVertex(node.nodeID, node);
    }
    for (const auto& nodeA : nodes)
    {
      for (const auto& nodeB : nodes)
      {
        if (nodeA.nodeID != nodeB.nodeID)
        {
          float cost = ComputeCost(nodeA, nodeB);
          weightGraph.AddEdge(nodeA.nodeID, nodeB.nodeID, cost);
        }
      }
    }
    return weightGraph;
  }

  virtual float ComputeCost(const NetworkNodeInfo& nodeA,
                            const NetworkNodeInfo& nodeB) = 0;
};
} // namespace AtlasNet::Network::Topology