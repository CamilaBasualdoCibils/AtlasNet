#pragma once
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include <boost/graph/adjacency_list.hpp>
#include "TopologyGraph.hpp"
namespace AtlasNet::Network::Topology
{
enum class TopologyConnectionState
{
  Connected,
  Disconnected,
  Connecting,
  Disconnecting
};
/* using TopologyWeightGraph = boost::adjacency_list<
    boost::vecS,                                 // vertex storage
    boost::vecS,                                 // edge storage
    boost::undirectedS,                          // undirected graph
    NetworkNodeInfo,                             // vertex properties
    boost::property<boost::edge_weight_t, float> // edge properties
    >;
using TopologyWeightVertex =
    boost::graph_traits<TopologyWeightGraph>::vertex_descriptor;
using TopologyWeightEdge =
    boost::graph_traits<TopologyWeightGraph>::edge_descriptor; */

using ConnectionGraph = TopologyGraph<AtlasNetNodeID, NetworkEdge>;
using WeightGraph = TopologyGraph<NetworkNodeInfo, boost::property<boost::edge_weight_t, float>>;
/* using ConnectionGraph =
    boost::adjacency_list<boost::vecS,        // vertex storage
                          boost::vecS,        // edge storage
                          boost::undirectedS, // undirected graph
                          AtlasNetNodeID,     // vertex properties
                          NetworkEdge         // edge properties
                          >;
using ConnectionVertex =
    boost::graph_traits<ConnectionGraph>::vertex_descriptor;
using ConnectionEdge = boost::graph_traits<ConnectionGraph>::edge_descriptor; */

struct ConnectInstruction
{
  AtlasNetNodeID a;
  AtlasNetNodeID b;
};
struct DisconnectInstruction
{
  AtlasNetNodeID a;
  AtlasNetNodeID b;
};
struct TransitionStep
{
  std::vector<ConnectInstruction> connect;
  std::vector<DisconnectInstruction> disconnect;
};
} // namespace AtlasNet::Network::Topology
