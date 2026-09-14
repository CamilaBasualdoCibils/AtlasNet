#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include <boost/graph/adjacency_list.hpp>
namespace AtlasNet::Network::Topology
{
template <typename Vertex, typename Edge> class TopologyGraph
{
public:
  using Graph = boost::adjacency_list<boost::vecS, boost::vecS,
                                      boost::undirectedS, Vertex, Edge>;

  using GraphVertex = boost::graph_traits<Graph>::vertex_descriptor;
  using GraphEdge = boost::graph_traits<Graph>::edge_descriptor;

private:
  Graph graph;

  std::unordered_map<AtlasNetNodeID, GraphVertex> nodeIDToVertexMap;

public:
void Clear()
  {
    graph.clear();
    nodeIDToVertexMap.clear();
  }
  const Graph& GetGraph() const
  {
    return graph;
  }

  bool ContainsNode(AtlasNetNodeID id) const
  {
    return nodeIDToVertexMap.contains(id);
  }

  Vertex GetVertex(AtlasNetNodeID id) const
  {
    return graph[nodeIDToVertexMap.at(id)];
  }
  Vertex GetVertex(GraphVertex vertex) const
  {
    return graph[vertex];
  }
  GraphVertex GetVertexDescriptor(AtlasNetNodeID id) const
  {
    return nodeIDToVertexMap.at(id);
  }
  std::vector<AtlasNetNodeID> GetNodes() const
  {
    std::vector<AtlasNetNodeID> result;

    for (auto v : boost::make_iterator_range(boost::vertices(graph)))
      result.push_back(graph[v]);

    return result;
  }

  std::vector<std::pair<AtlasNetNodeID, AtlasNetNodeID>> GetConnections() const
  {
    std::vector<std::pair<AtlasNetNodeID, AtlasNetNodeID>> result;

    for (auto e : boost::make_iterator_range(boost::edges(graph)))
    {
      auto a = graph[boost::source(e, graph)];
      auto b = graph[boost::target(e, graph)];

      result.emplace_back(std::minmax(a, b));
    }

    return result;
  }

  bool HasEdge(AtlasNetNodeID a, AtlasNetNodeID b) const
  {
    auto va = GetVertex(a);
    auto vb = GetVertex(b);

    return boost::edge(va, vb, graph).second;
  }
  void AddVertex(AtlasNetNodeID id, Vertex properties = {})
  {
    auto v = boost::add_vertex(properties, graph);
    nodeIDToVertexMap[id] = v;
  }
  void RemoveVertex(AtlasNetNodeID id)
  {
    auto v = GetVertex(id);
    boost::clear_vertex(v, graph);
    boost::remove_vertex(v, graph);
    nodeIDToVertexMap.erase(id);
  }
  void AddEdge(AtlasNetNodeID a, AtlasNetNodeID b, Edge properties = {})
  {
    boost::add_edge(GetVertexDescriptor(a), GetVertexDescriptor(b), properties, graph);
  }
  

  void RemoveEdge(AtlasNetNodeID a, AtlasNetNodeID b)
  {
    boost::remove_edge(GetVertexDescriptor(a), GetVertexDescriptor(b), graph);
  }

  /* std::vector<AtlasNetNodeID> GetNeighbors(AtlasNetNodeID id) const
  {
    std::vector<AtlasNetNodeID> result;

    auto v = GetVertex(id);

    for (auto e : boost::make_iterator_range(boost::out_edges(v, graph)))
    {
      auto other = boost::target(e, graph);

      if (other == v)
        other = boost::source(e, graph);

      result.push_back(graph[other]);
    }

    return result;
  } */
  auto GetVerticies() const
  {
    
    //return a view of only the keys of NodeIDToVertexMap
    std::vector<NetworkNodeInfo> result;
    for (const auto& [id, vertex] : nodeIDToVertexMap)
    {
      result.push_back(graph[vertex]); 
    }
    return result;
  }
  auto GetEdges() const
  {
    std::vector<std::pair<AtlasNetNodeID, AtlasNetNodeID>> result;

    for (auto e : boost::make_iterator_range(boost::edges(graph)))
    {
      auto a = graph[boost::source(e, graph)];
      auto b = graph[boost::target(e, graph)];

      result.emplace_back(std::minmax(a, b));
    }

    return result;
  }
};
} // namespace AtlasNet::Network::Topology