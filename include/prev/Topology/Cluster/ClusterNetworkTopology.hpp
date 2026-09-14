#pragma once

#include "atlasnet/core/entity/EntityLedger.hpp"
#include "atlasnet/core/network/topology/INetworkTopology.hpp"
#include "atlasnet/core/network/topology/TopologyCommons.hpp"
#include <boost/graph/kruskal_min_spanning_tree.hpp>
namespace AtlasNet::Network::Topology
{
class ClusterNetworkTopology : public INetworkTopology
{
  TopologyWeightedGraph weightedGraph;
  TopologyConnectionGraph connectionGraph;

  boost::bimap<AtlasNetNodeID, TopologyWeightedVertex> nodeMap;
  std::set<std::pair<TopologyWeightedVertex, TopologyWeightedVertex>>
      currentEdges;

public:
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
    return weightedGraph;
  }

  void AddNode(const NetworkNodeInfo& node) override
  {
    auto r = boost::add_vertex(node, weightedGraph);
    nodeMap.insert({node.nodeID, r});
  }

  void RemoveNode(const NetworkNodeID& node) override
  {
    boost::remove_vertex(nodeMap.left.at(node), weightedGraph);
    nodeMap.left.erase(node);
  }

  void Parse() override
  {
    weightedGraph.m_edges.clear();
    std::vector<TopologyWeightedVertex> vertices;
    for (auto v : boost::make_iterator_range(boost::vertices(weightedGraph)))
    {
      vertices.push_back(v);
    }
    for (size_t i = 0; i < vertices.size(); ++i)
    {
      for (size_t j = i + 1; j < vertices.size(); ++j)
      {

        auto& nodeA = weightedGraph[vertices[i]];
        auto& nodeB = weightedGraph[vertices[j]];
        float weight = 100.0f;
        if (nodeA.serverID == nodeB.serverID)
          weight -= 50.0f;

        if (nodeA.rack == nodeB.rack)
          weight -= 25.0f;

        if (nodeA.region == nodeB.region)
          weight -= 10.0f;

        boost::add_edge(vertices[i], vertices[j], weight, weightedGraph);
      }
    }
  }

  /* TopologyDiff ComputeDiff() override
  {
    // Compute MST
    std::vector<TopologyWeightedEdge> mst;
    boost::kruskal_minimum_spanning_tree(weightedGraph,
                                         std::back_inserter(mst));

    // Build a new connection graph
    TopologyConnectionGraph nextGraph;

    // Copy all vertices first
    std::unordered_map<TopologyWeightedVertex, TopologyConnectionVertex> map;

    for (auto v : boost::make_iterator_range(boost::vertices(weightedGraph)))
    {
      auto nv = boost::add_vertex(weightedGraph[v], nextGraph);
      map.emplace(v, nv);
    }

    // Add MST edges
    for (auto e : mst)
    {
      auto u = boost::source(e, weightedGraph);
      auto v = boost::target(e, weightedGraph);

      boost::add_edge(map[u], map[v], NetworkEdge{}, nextGraph);
    }

    TopologyDiff diff;

    // TODO:
    // Compare connectionGraph against nextGraph
    // Fill diff.connect
    // Fill diff.disconnect

    connectionGraph = std::move(nextGraph);

    return diff;
  } */
};
} // namespace AtlasNet::Network::Topology