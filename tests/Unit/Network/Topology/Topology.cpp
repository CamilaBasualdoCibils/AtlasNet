

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyAssigner.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyDeployer.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyPlanner.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyPolicy.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyTransitionPlanner.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSock.hpp"
#include "TopologyRenderer.hpp"
#include "boost/graph/graphviz.hpp"
#include <boost/graph/connected_components.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
using namespace AtlasNet;
using namespace AtlasNet::Network::Topology;
/* class MockTopologyExecutor : public ITopologyExecutor
{

public:
  MockTopologyExecutor() = default;
  MOCK_METHOD(bool, SendConnectCommand,
              (NetworkNodeID a, NetworkNodeID b),
              (override));

  MOCK_METHOD(bool, SendDisconnectCommand,
              (NetworkNodeID a, NetworkNodeID b),
              (override));
  MOCK_METHOD(AtlasNet::Network::SocketConnectionState, GetConnectionState,
              (NetworkNodeID a, NetworkNodeID b), (const, override));
}; */
/* void ApplyStep(ConnectionGraph& graph, const TransitionStep& step)
{
  auto findVertex = [&](AtlasNetNodeID id) -> ConnectionVertex
  {
    for (auto v : boost::make_iterator_range(vertices(graph)))
    {
      if (graph[v] == id)
        return v;
    }

    auto v = boost::add_vertex(graph);
    graph[v] = id;
    return v;
  };

  // Connect first
  for (const auto& c : step.connect)
  {
    auto a = findVertex(c.a);
    auto b = findVertex(c.b);

    boost::add_edge(a, b, graph);
  }

  // Disconnect afterwards
  for (const auto& d : step.disconnect)
  {
    auto a = findVertex(d.a);
    auto b = findVertex(d.b);

    boost::remove_edge(a, b, graph);
  }
}
bool IsConnected(const ConnectionGraph& graph)
{
  if (boost::num_vertices(graph) == 0)
    return true;
  std::vector<int> component(boost::num_vertices(graph));
  int num = boost::connected_components(graph, &component[0]);
  return num == 1;
}
std::vector<Network::NetworkNodeInfo> GenerateTestNodes(size_t numNodes,
                                                        size_t numServers)
{
  std::vector<Network::NetworkNodeInfo> nodes;
  std::vector<std::string> servers;
  for (size_t i = 0; i < numServers; i++)
  {
    servers.push_back(std::format("Server {}", i));
  }

  for (size_t i = 0; i < numNodes; ++i)
  {
    Network::NetworkNodeInfo node{
        AtlasNet::AtlasNetNodeID(i), 0.0f, servers[i % servers.size()],
        "rack" + std::to_string(i), "region" + std::to_string(i)};
    nodes.push_back(node);
  }
  return nodes;
} */
class TestTopologyPolicy : public AtlasNet::Network::Topology::ITopologyPolicy
{

  float ComputeCost(const Network::NetworkNodeInfo& nodeA,
                    const Network::NetworkNodeInfo& nodeB) override
  {
    // Compute cost logic here
    return 0.0f;
  }
};
class TestTopologyExecutor
    : public AtlasNet::Network::Topology::ITopologyDeployer
{
  std::future<bool> Connect(AtlasNetNodeID a, AtlasNetNodeID b) override {}
  std::future<bool> Disconnect(AtlasNetNodeID a, AtlasNetNodeID b) override {}
};
class TestTopologyPlanner : public AtlasNet::Network::Topology::ITopologyPlanner
{
  bool Mesh = true;

public:
  TestTopologyPlanner(bool mesh = true) : Mesh(mesh) {}
  ConnectionGraph Compute(const WeightGraph& nodes) override
  {
    if (Mesh)
    {
      return ComputeMeshTopology(nodes);
    }
    else
    {
      return ComputeMSTTopology(nodes);
    }
  }
  ConnectionGraph ComputeMeshTopology(const WeightGraph& weightGraph)
  {

    ConnectionGraph nextGraph;

    // Copy all vertices first
    // std::unordered_map<TopologyWeightVertex, ConnectionVertex> map;

    for (auto v : weightGraph.GetVerticies())
    {
      nextGraph.AddVertex(weightGraph.GetVertex(v.nodeID).nodeID, v.nodeID);
    }

    // Add edges between all pairs of vertices
    for (auto u : weightGraph.GetVerticies())
    {
      for (auto v : weightGraph.GetVerticies())
      {
        if (u.nodeID != v.nodeID)
        {
          nextGraph.AddEdge(u.nodeID, v.nodeID, Network::NetworkEdge{});
        }
      }
    }

    return nextGraph;
  }
  ConnectionGraph ComputeMSTTopology(const WeightGraph& weightGraph)
  {
    // Compute MST
    std::vector<WeightGraph::GraphEdge> mst;
    boost::kruskal_minimum_spanning_tree(weightGraph.GetGraph(),
                                         std::back_inserter(mst));

    // Build a new connection graph
    ConnectionGraph nextGraph;

    // Copy all vertices first
    std::unordered_map<WeightGraph::GraphVertex, ConnectionGraph::GraphVertex>
        map;

    for (auto v : weightGraph.GetVerticies())
    {
      nextGraph.AddVertex(v.nodeID);
     /*  auto nv = boost::add_vertex(weightGraph[v].nodeID, nextGraph);
      map.emplace(v, nv);
      nextGraph.AddVertex(weightGraph[v].nodeID,
                          weightGraph.GetVertex(weightGraph[v].nodeID)); */
    }

    // Add MST edges
    for (auto e : mst)
    {
      auto u = weightGraph.GetVertex( boost::source(e, weightGraph.GetGraph()));
      auto v =weightGraph.GetVertex( boost::target(e, weightGraph.GetGraph()));
      nextGraph.AddEdge(u.nodeID, v.nodeID, Network::NetworkEdge{});

      //boost::add_edge(map[u], map[v], Network::NetworkEdge{}, nextGraph);
    }
    return nextGraph;
  }
};

TEST_F(TopologyRenderer, Render)
{
  SetNumNodes(10);
  SetPlanner(std::make_shared<TestTopologyPlanner>(true));
  SetTransport(std::make_shared<AtlasNet::Network::SteamNetSockTransport>());
  Start();

  std::this_thread::sleep_for(std::chrono::seconds(5));
  Finish();
}
/* TEST(Topology, TopologyTransition)
{
  using ID = AtlasNet::Network::NetworkNodeID;
  MockTopologyExecutor executor;
  TopologyTransition transition(executor);
  auto action0 = transition.AddConnectAction(ID(1), ID(2));
  auto action1 = transition.AddWaitConnectAction(ID(1), ID(2));
  transition.DependsOn(action0, action1);

  EXPECT_CALL(executor, SendConnectCommand(ID(1), ID(2))).Times(1);
  transition.Execute();
}
TEST(Topology, Mesh)
{
  AtlasNet::Network::Topology::MeshNetworkTopology topology;

  for (int i = 0; i < 5; ++i)
  {
    NetworkNodeInfo node{
        AtlasNet::AtlasNetNodeID(i), "server" + std::to_string(i),
        "rack" + std::to_string(i), "region" + std::to_string(i)};
    topology.AddNode(node);
  }
  topology.Parse();

}
struct VertexWriter
{
  const Topology::TopologyConnectionGraph& graph;

  VertexWriter(const Topology::TopologyConnectionGraph& g) : graph(g) {}

  template <typename Vertex>
  void operator()(std::ostream& out, const Vertex& v) const
  {
    const auto& node = graph[v];

    out << "[label=\"" << node.nodeID << "\\n" << node.serverID << "\"]";
  }
};
TEST(Topology, Cluster)
{

  AtlasNet::Network::Topology::ClusterNetworkTopology topology;
  const size_t numServers = 5;
  std::vector<std::string> servers;
  for (size_t i = 0; i < numServers; i++)
  {
    servers.push_back(std::format("Server {}", i));
  }

  const size_t numNodes = 50;
  for (size_t i = 0; i < numNodes; ++i)
  {
    NetworkNodeInfo node{AtlasNet::AtlasNetNodeID(i),
                         servers[i % servers.size()]

    };
    topology.AddNode(node);
  }

  topology.Parse();
     Topology::TopologyDiff diff = topology.ComputeDiff();
    boost::write_graphviz(std::cout, topology.GetConnectionGraph(),
                          VertexWriter(topology.GetConnectionGraph()));
}*/