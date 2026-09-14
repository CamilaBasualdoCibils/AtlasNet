#pragma once
#include "atlasnet/core/network/topology/INetworkTopology.hpp"
#include "atlasnet/core/network/topology/TopologyCommons.hpp"
namespace AtlasNet::Network::Topology
{
class MeshNetworkTopology : public INetworkTopology
{
    TopologyWeightedGraph graph;
    TopologyConnectionGraph connectionGraph;

  public:
    void RemoveNode(const NetworkNodeID&) override
    {

    }

    TopologyTransition CreateTransition(ITopologyExecutor& executor, const TopologyConnectionGraph& graph) override
    {
      return TopologyTransition(executor);
    }

    const TopologyConnectionGraph& GetConnectionGraph() const override
    {
      return connectionGraph;
    }

    const TopologyWeightedGraph& GetWeightedGraph() const override
    {
      // TODO: Implement this pure virtual method.
        return graph;
    }

  void AddNode(const NetworkNodeInfo&) override
  {

  }

  void Parse() override
  {

  }

  
};
} // namespace AtlasNet::Network::Topology