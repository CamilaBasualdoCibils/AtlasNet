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
#include "AtlasNet/DB/Backend/IDatabaseBackend.hpp"
#include "AtlasNet/Node/NodeConfig.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <spdlog/logger.h>
#include <stop_token>
#include <thread>
namespace AtlasNet
{
class AtlasNetNode final
{
private:
  NodeConfig nodeConfig;
  const AtlasNetNodeID nodeID;
  std::shared_ptr<spdlog::logger> logger;
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
  explicit AtlasNetNode(NodeConfig config,
                        std::unique_ptr<DB::IDatabaseBackend> backend = {});
  const NodeConfig& GetConfig() const
  {
    return nodeConfig;
  }
  void Start();
  void Poll();
  // Borrowed until node destruction. Hosts remove their event registration
  // first.
  int GetHandshakePollDescriptor() const
  {
    return HandshakeTransport ? HandshakeTransport->GetPollDescriptor() : -1;
  }
  void Run(std::stop_token stop = {});
  void RequestStop()
  {
    GetLogger()->info("Shutdown Requested...");
    stop_requested.store(true);
  }

  ~AtlasNetNode();

  auto GetLogger() const -> std::shared_ptr<spdlog::logger>
  {
    return logger;
  }

private:
  Network::SocketAddress GetClusterListenAddress() const
  {
    return baseTransport->GetListenAddress();
  }
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
  bool started = false;
  void Initialize();
  void Tick();
  void InitializeChannels();
  void Shutdown();
  std::unique_ptr<DB::IDatabaseBackend> database;
};
} // namespace AtlasNet
