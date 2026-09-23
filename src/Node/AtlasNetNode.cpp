#include "AtlasNet/Node/AtlasNetNode.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "AtlasNet/Node/NodeCapability.hpp"
#include <boost/describe/enum_to_string.hpp>
#include <future>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#ifdef ATLASNET_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

AtlasNet::AtlasNetNode::AtlasNetNode(
    NodeConfig config, std::unique_ptr<DB::IDatabaseBackend> backend)
    : nodeConfig(std::move(config)), nodeID(AtlasNetNodeID::Generate()),
      database(std::move(backend))
{
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Database) &&
      !database)
    throw std::invalid_argument(
        "Database capability requires an injected backend");
  if (!nodeConfig.ingressSockets.empty() &&
      !HasCapability(nodeConfig.capabilities, NodeCapability::ClientIngress))
    throw std::invalid_argument(
        "Ingress sockets require ClientIngress capability");
  logger = spdlog::stdout_color_mt("AtlasNet:" + nodeID.to_short_string());
}

void AtlasNet::AtlasNetNode::Tick()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::Tick");
#endif
  if (HandshakeRPC && HandshakeTransport)
  {
    GetHandshakeRPC().Poll(Network::PollType::NonBlocking);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

void AtlasNet::AtlasNetNode::Initialize()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::Initialize");
#endif

  for (const auto& socket : nodeConfig.ingressSockets)
  {
    GetLogger()->info("Ingress Socket: {}:{} {}",
                      boost::describe::enum_to_string(socket.type, "<INVALID>"),
                      socket.port, socket.ExtraArgs);
  }
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Database))
  {
    GetHandshakeRPC().Bind<RPC::Database::Ping>(
        [](const Network::RPC::NetworkTransportRPC::CallContext&, int)
        { return 0; });
    GetHandshakeRPC().Bind<RPC::Database::RegisterNode>(
        [this](const Network::RPC::NetworkTransportRPC::CallContext&,
               const RPC::Database::RegisterNodeRequest& request)
        {
          try
          {
            return database->RegisterNode(request);
          }
          catch (const std::exception& e)
          {
            GetLogger()->error("Registry write failed: {}", e.what());
            return RPC::Database::RegisterNodeResponse::FAILURE;
          }
        });
    RPC::Database::RegisterNodeRequest self;
    self.nodeID = GetNodeID();
    self.handshakeAddress = GetHandshakeTransport().GetListenAddress();
    self.capabilities = nodeConfig.capabilities;
    self.channelBusAddress = GetClusterListenAddress();
    if (database->RegisterNode(self) !=
        RPC::Database::RegisterNodeResponse::SUCCESS)
      throw std::runtime_error("Failed to register database node");
  }
  if (!nodeConfig.dbHandshakeAddress.IsValid())
    return;

  {
    // Poll on the host's thread so injected backends retain their threading
    // contract.
    const auto waitFor = [this](auto& future, std::chrono::seconds timeout)
    {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      while (future.wait_for(std::chrono::seconds(0)) !=
                 std::future_status::ready &&
             std::chrono::steady_clock::now() < deadline)
      {
        GetHandshakeRPC().Poll(Network::PollType::NonBlocking);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      return future.wait_for(std::chrono::seconds(0));
    };
    const int pingMaxAttempts = 10;
    for (int i = 0; i < pingMaxAttempts; i++)
    {
      GetLogger()->info("Pinging DB at {}... attempt {}/{}",
                        nodeConfig.dbHandshakeAddress.to_string(), i + 1,
                        pingMaxAttempts);

      auto pingResult = GetHandshakeRPC().Call<RPC::Database::Ping>(
          nodeConfig.dbHandshakeAddress, 0);
      std::future_status status = waitFor(pingResult, std::chrono::seconds(1));
      bool Successful = status == std::future_status::ready;
      if (Successful)
      {
        Network::RPC::TRPCResult<int> result = pingResult.get();
        if (result.has_value())
        {
          GetLogger()->info("Successfully pinged DB at {}",
                            nodeConfig.dbHandshakeAddress.to_string());
          break;
        }
        else
        {
          Successful = false;
        }
      }

      if (!Successful)
      {

        if (i == pingMaxAttempts - 1)
        {
          GetLogger()->error(
              "Failed to ping DB at {} after {} attempts. Exiting.",
              nodeConfig.dbHandshakeAddress.to_string(), pingMaxAttempts);
          throw std::runtime_error(
              "Failed to ping DB after multiple attempts.");
        }
        GetLogger()->warn("Failed to ping DB at {}. Retrying...",
                          nodeConfig.dbHandshakeAddress.to_string());
      }
    }
    RPC::Database::RegisterNodeRequest registerRequest;
    registerRequest.handshakeAddress =
        GetHandshakeTransport().GetListenAddress();
    registerRequest.capabilities = nodeConfig.capabilities;
    registerRequest.channelBusAddress = GetClusterListenAddress();
    registerRequest.nodeID = GetNodeID();
    std::future<Network::RPC::TRPCResult<RPC::Database::RegisterNodeResponse>>
        response = GetHandshakeRPC().Call<RPC::Database::RegisterNode>(
            nodeConfig.dbHandshakeAddress, registerRequest);

    std::future_status registerStatus =
        waitFor(response, std::chrono::seconds(5));
    if (registerStatus != std::future_status::ready)
    {
      GetLogger()->error(
          "Failed to register node with DB at {}. Timeout after 5 seconds.",
          nodeConfig.dbHandshakeAddress.to_string());
      throw std::runtime_error("Failed to register node with DB.");
    }
    Network::RPC::TRPCResult<RPC::Database::RegisterNodeResponse>
        registerResult = response.get();
    if (!registerResult.has_value())
    {
      GetLogger()->error(
          "Failed to register node with DB at {}. Error: {}",
          nodeConfig.dbHandshakeAddress.to_string(),
          boost::describe::enum_to_string(registerResult.error(), "<INVALID>"));
      throw std::runtime_error("Failed to register node with DB.");
    }
    if (registerResult.value() != RPC::Database::RegisterNodeResponse::SUCCESS)
      throw std::runtime_error("Database rejected node registration");
    GetLogger()->info("Successfully registered node with DB at {}",
                      nodeConfig.dbHandshakeAddress.to_string());
    if (!HasCapability(nodeConfig.capabilities,
                       AtlasNet::NodeCapability::Database))
    {
      GetLogger()->info(
          "Node has no Database capability, shutting down handshake socket.");
      HandshakeRPC.reset();
      HandshakeTransport.reset();
    }
  }
}

void AtlasNet::AtlasNetNode::Start()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::Start");
#endif
  if (started)
    throw std::logic_error("Node already started");
  switch (nodeConfig.transport.networkTransportType)
  {

  case Network::Cluster::ClusterTransportType::INVALID:
    throw std::runtime_error("Invalid cluster transport type.");
  case Network::Cluster::ClusterTransportType::UDP:
    baseTransport = std::make_shared<Network::UDPNetworkTransport>(
        "ChannelBusNetworkTransport:" + nodeID.to_short_string(),
        Network::SocketAddress(Network::IPv6::Any(),
                               nodeConfig.transport.clusterListenPort));
    break;
  case Network::Cluster::ClusterTransportType::DPDK:
    throw std::runtime_error("DPDK cluster transport is not yet implemented.");
    break;
  }
  logger->info("Channel Bus listening on {}", baseTransport->GetListenPort());
  /* clusterTransport = std::make_shared<Network::Cluster::ClusterTransport>(
      baseTransport, nullptr); */

  HandshakeTransport = std::make_shared<Network::UDPNetworkTransport>(
      "HandshakeTransport:" + nodeID.to_short_string(),
      Network::SocketAddress(
          Network::IPv6::Any(),
          nodeConfig.transport.handshakeListenPort)); // Use ephemeral port for
                                                      // handshake transport
  HandshakeRPC = std::make_shared<Network::RPC::NetworkTransportRPC>(
      "HandshakeRPC:" + nodeID.to_short_string(),
      Network::RPC::NetworkTransportRPC::Config{.networkTransport =
                                                    HandshakeTransport});
  logger->info("Handshake port: {}", HandshakeTransport->GetListenPort());
  InitializeChannels();
  // Initialize capability-specific behavior
  Initialize();

  started = true;
}

void AtlasNet::AtlasNetNode::Poll()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::Poll");
#endif
  if (!started)
    throw std::logic_error("Node has not started");
  Tick();
}

void AtlasNet::AtlasNetNode::Run(std::stop_token stop)
{
#ifdef ATLASNET_TRACY_ENABLED
  tracy::SetThreadName("AtlasNet Node");
  ZoneScopedN("AtlasNetNode::Run");
#endif
  Start();
  while (!stop.stop_requested() && !stop_requested.load())
  {
    Poll();
#ifdef ATLASNET_TRACY_ENABLED
    FrameMarkNamed("AtlasNet Node Poll");
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  GetLogger()->info("Shutting Down...");
  Shutdown();
  GetLogger()->info("Goodbye");
}

void AtlasNet::AtlasNetNode::InitializeChannels()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::InitializeChannels");
#endif
  channelBus = std::make_shared<Network::Cluster::ChannelBus>(
      Network::Cluster::ChannelBus::ChannelBusOptions{
          .transport = clusterTransport,
          .name = "ChannelBus:" + nodeID.to_string()});
}
void AtlasNet::AtlasNetNode::Shutdown() {}

AtlasNet::AtlasNetNode::~AtlasNetNode()
{
  for (const auto* prefix :
       {"AtlasNet:", "ChannelBusNetworkTransport:", "HandshakeTransport:",
        "HandshakeRPC:", "ChannelBus:"})
    spdlog::drop(prefix + nodeID.to_string());
}
