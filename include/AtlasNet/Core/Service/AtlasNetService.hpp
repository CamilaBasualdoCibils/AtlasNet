#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelBus.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ReservedChannels.hpp"
#include "AtlasNet/Core/Network/Cluster/ClusterCommons.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
#include "AtlasNet/Core/Network/Ingress/IngressCommons.hpp"
#include "AtlasNet/Core/Network/Intent/ClusterIntentChannel.hpp"
#include "AtlasNet/Core/Network/RPC/NetworkTransportRPC.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include <boost/describe.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/stacktrace.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <chrono>
#include <memory>
#include <set>
#include <spdlog/logger.h>
#include <thread>
#include <unordered_set>
namespace AtlasNet
{
enum class AtlasNetServiceType
{
  Node,
  DB,
  Gateway
};
BOOST_DESCRIBE_ENUM(AtlasNetServiceType, Node, DB, Gateway);
class AtlasNetService
{
public:
  struct Options
  {

    Network::Cluster::ClusterTransportType networkTransportType =
        Network::Cluster::ClusterTransportType::INVALID;
    Network::PortType clusterListenPort = Network::PORT_EPHEMERAL;
    Network::PortType handshakeListenPort = Network::PORT_INVALID;
  };

private:
  Options options;
  const AtlasNetServiceType service_type;
  std::shared_ptr<spdlog::logger> logger;
  const int signalFd;
  const AtlasNetNodeID nodeID;
  std::atomic_bool stop_requested{false};
  
  std::shared_ptr<Network::INetworkTransport> HandshakeTransport;
  std::shared_ptr<Network::RPC::NetworkTransportRPC> HandshakeRPC; 
  
  std::shared_ptr<Network::INetworkTransport> baseTransport;
  std::shared_ptr<Network::Cluster::ClusterTransport> clusterTransport;
  std::shared_ptr<Network::Cluster::ChannelBus> channelBus;

  std::unordered_map<Network::Cluster::ReservedChannels,
                     std::shared_ptr<Network::Intent::ClusterIntentChannel>>
      clusterIntentChannels;

public:
  AtlasNetService(AtlasNetServiceType service_type, int argc, char** argv);
  void Run();

  virtual ~AtlasNetService() = default;

  auto GetLogger() const -> std::shared_ptr<spdlog::logger>
  {
    return logger;
  }

protected:
  virtual void AddOptions(boost::program_options::options_description& desc);
  virtual void ParseOptions(const boost::program_options::variables_map& vm);
  Network::Cluster::ClusterTransport& GetClusterTransport() const
  {
    return *clusterTransport;
  }

  Network::INetworkTransport& GetHandshakeTransport() const
  {
    assert(HandshakeTransport != nullptr);
    return *HandshakeTransport;
  }
  Network::RPC::NetworkTransportRPC& GetHandshakeRPC() const
  {
    assert(HandshakeRPC != nullptr);
    return *HandshakeRPC;
  }
  AtlasNetNodeID GetNodeID() const
  {
    return nodeID;
  }
private:
  void MainLoop();
  virtual void Initialize() = 0;
  virtual void Tick() = 0;
  void InitializeChannels();

  static std::string GetHostID();
  static int SetupSignals();
  std::optional<int> CheckForSignal();


  const int argc;
  char const* const* argv;
};
} // namespace AtlasNet