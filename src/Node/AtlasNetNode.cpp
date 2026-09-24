#include "AtlasNet/Node/AtlasNetNode.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "AtlasNet/Node/NodeCapability.hpp"
#include "Node/Controller/Controller.hpp"
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <future>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>
#ifdef ATLASNET_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

namespace
{
class RuntimeResolver final
    : public AtlasNet::Network::Cluster::IClusterResolver,
      public AtlasNet::Network::Intent::IIntentResolver
{
public:
  void AddNode(AtlasNet::AtlasNetNodeID id,
               AtlasNet::Network::SocketAddress address)
  {
    std::scoped_lock lock(mutex);
    addresses.insert_or_assign(id.to_string(), address);
    nodesByAddress.insert_or_assign(address.to_string(), id);
  }

  void SetDatabase(AtlasNet::AtlasNetNodeID id)
  {
    std::scoped_lock lock(mutex);
    database = id;
  }

  std::optional<AtlasNet::Network::SocketAddress>
  ResolveNodeAddress(AtlasNet::AtlasNetNodeID id) override
  {
    std::scoped_lock lock(mutex);
    const auto it = addresses.find(id.to_string());
    return it == addresses.end() ? std::nullopt
                                 : std::optional(it->second);
  }

  std::optional<AtlasNet::Network::MACAddress>
  ResolveNodeMAC(AtlasNet::AtlasNetNodeID) override
  {
    return std::nullopt;
  }

  std::optional<AtlasNet::AtlasNetNodeID> ResolveNodeID(
      const AtlasNet::Network::SocketAddress& address) override
  {
    std::scoped_lock lock(mutex);
    const auto it = nodesByAddress.find(address.to_string());
    return it == nodesByAddress.end() ? std::nullopt
                                      : std::optional(it->second);
  }

  std::optional<AtlasNet::AtlasNetNodeID>
  ResolveNodeID(const AtlasNet::Network::MACAddress&) override
  {
    return std::nullopt;
  }

  std::optional<AtlasNet::AtlasNetNodeID> ResolveIntent(
      const AtlasNet::Network::Intent::VIntent& intent) override
  {
    std::scoped_lock lock(mutex);
    return std::visit(
        [this](const auto& recipient) -> std::optional<AtlasNet::AtlasNetNodeID>
        {
          using T = std::decay_t<decltype(recipient)>;
          if constexpr (std::is_same_v<
                            T, AtlasNet::Network::Intent::Recepient::
                                   NodeRecepient>)
            return recipient.nodeID;
          else if constexpr (std::is_same_v<
                                 T, AtlasNet::Network::Intent::Recepient::
                                        DatabaseRecepient>)
            return database;
          else
            return std::nullopt;
        },
        intent);
  }

private:
  std::mutex mutex;
  std::unordered_map<std::string, AtlasNet::Network::SocketAddress> addresses;
  std::unordered_map<std::string, AtlasNet::AtlasNetNodeID> nodesByAddress;
  std::optional<AtlasNet::AtlasNetNodeID> database;
};
} // namespace

AtlasNet::AtlasNetNode::AtlasNetNode(
    NodeConfig config, std::unique_ptr<DB::IDatabaseBackend> backend)
    : nodeConfig(std::move(config)), nodeID(AtlasNetNodeID::Generate()),
      database(std::move(backend))
{
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Database) &&
      !database)
    throw std::invalid_argument(
        "Database capability requires an injected backend");
  if (!nodeConfig.clientIngressListeners.empty() &&
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
  if (channelBus && intentRPC)
  {
    channelBus->TryReceive();
    intentRPC->Poll(Network::PollType::NonBlocking);
  }
  if (shardManager)
    shardManager->Poll();
  std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

void AtlasNet::AtlasNetNode::Initialize()
{
#ifdef ATLASNET_TRACY_ENABLED
  ZoneScopedN("AtlasNetNode::Initialize");
#endif

  if (HasCapability(nodeConfig.capabilities, NodeCapability::Database))
  {
    GetHandshakeRPC().Bind<RPC::Database::Ping>(
        [this](const Network::RPC::NetworkTransportRPC::CallContext&, int)
        {
          return RPC::Database::StartupInfo{
              .nodeID = GetNodeID(),
              .channelBusAddress = GetClusterListenAddress()};
        });
    GetHandshakeRPC().Bind<RPC::Database::RegisterNode>(
        [this](const Network::RPC::NetworkTransportRPC::CallContext& context,
               const RPC::Database::RegisterNodeRequest& request)
        {
          try
          {
            auto address = context.caller;
            address.set_port(request.channelBusAddress.get_port());
            std::static_pointer_cast<RuntimeResolver>(clusterResolver)
                ->AddNode(request.nodeID, address);
            return database->RegisterNode(request);
          }
          catch (const std::exception& e)
          {
            GetLogger()->error("Registry write failed: {}", e.what());
            return RPC::Database::RegisterNodeResponse::FAILURE;
          }
        });
    intentRPC->Bind<RPC::Database::ClaimControllerPromotion>(
        [this](const Network::RPC::ClusterIntentRPC::CallContext&,
               AtlasNetNodeID claimant)
        {
          try
          {
            return database->ClaimControllerPromotion(claimant);
          }
          catch (const std::exception& e)
          {
            GetLogger()->error("Controller promotion claim failed: {}",
                               e.what());
            return RPC::Database::ClaimControllerPromotionResponse::FAILURE;
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
  {
    TryClaimControllerPromotion();
    return;
  }

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
        Network::RPC::TRPCResult<RPC::Database::StartupInfo> result =
            pingResult.get();
        if (result.has_value())
        {
          auto address = nodeConfig.dbHandshakeAddress;
          address.set_port(result.value().channelBusAddress.get_port());
          auto resolver =
              std::static_pointer_cast<RuntimeResolver>(clusterResolver);
          resolver->AddNode(result.value().nodeID, address);
          resolver->SetDatabase(result.value().nodeID);
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
    TryClaimControllerPromotion();
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
  InitializeModules();
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Shard))
  {
    auto worker = nodeConfig.shardWorkerExecutable;
    if (worker.empty())
      worker = std::filesystem::read_symlink("/proc/self/exe").parent_path() /
               "AtlasNetShardWorker";
    shardManager = std::make_unique<ShardManager>(
        ShardManager::Config{.workerExecutable = std::move(worker),
                             .modules = nodeConfig.modules});
    shardManager->CreateShard();
  }
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
  auto runtimeResolver = std::make_shared<RuntimeResolver>();
  runtimeResolver->AddNode(
      GetNodeID(), Network::SocketAddress(Network::IPv6::Loopback(),
                                         baseTransport->GetListenPort()));
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Database))
    runtimeResolver->SetDatabase(GetNodeID());
  clusterResolver = runtimeResolver;
  intentResolver = runtimeResolver;
  clusterTransport = std::make_shared<Network::Cluster::ClusterTransport>(
      baseTransport, clusterResolver);

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
  for (auto& listener : ingressListeners)
    listener->Poll();
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
  auto channel = channelBus->MakeChannel(Network::Cluster::ChannelOptions{
      .id = static_cast<Network::Cluster::ChannelID>(
          Network::Cluster::ReservedChannels::INTENT_RPC),
      .delivery = Network::Cluster::DeliveryMode::Reliable,
      .ordering = Network::Cluster::OrderingMode::Ordered,
      .batching = Network::Cluster::BatchMode::Immediate});
  auto intentChannel = std::make_shared<Network::Intent::ClusterIntentChannel>(
      GetNodeID(), std::move(channel), intentResolver);
  clusterIntentChannels.emplace(Network::Cluster::ReservedChannels::INTENT_RPC,
                                intentChannel);
  intentRPC = std::make_shared<Network::RPC::ClusterIntentRPC>(
      "IntentRPC:" + nodeID.to_short_string(),
      Network::RPC::ClusterIntentRPC::Config{
          .clusterIntentChannel = std::move(intentChannel)});
}
void AtlasNet::AtlasNetNode::InitializeModules()
{
  for (const auto& path : nodeConfig.modules)
  {
    const auto& module = moduleLoader.Load(path, moduleRegistry);
    GetLogger()->info("Loaded module {} {} from {}", module.name,
                      module.version, path);
  }
  if (HasCapability(nodeConfig.capabilities, NodeCapability::Shard) &&
      !moduleRegistry.HasShardProvider())
  {
    GetLogger()->critical("Node has Shard capability enabled, but no loaded "
                          "module provides shard logic.");
    throw std::runtime_error(
        "Shard capability requires a module-provided ShardProvider");
  }
  for (const auto& requested : nodeConfig.clientIngressListeners)
  {
    auto provider =
        moduleRegistry.GetClientIngressTransport(requested.transport);
    if (!provider)
      throw std::runtime_error(
          "No loaded module provides client ingress transport '" +
          requested.transport + "'");
    auto listener = provider->CreateListener(requested.config);
    if (!listener)
      throw std::runtime_error("Client ingress provider '" +
                               requested.transport +
                               "' returned a null listener");
    GetLogger()->info("Created client ingress listener for transport '{}'",
                      requested.transport);
    listener->Start();
    ingressListeners.push_back(std::move(listener));
  }
}
void AtlasNet::AtlasNetNode::Shutdown()
{
  if (shardManager)
  {
    shardManager->Shutdown();
    shardManager.reset();
  }
  if (controller)
    controller->Stop();
  for (auto it = ingressListeners.rbegin(); it != ingressListeners.rend(); ++it)
    (*it)->Shutdown();
  ingressListeners.clear();
}

void AtlasNet::AtlasNetNode::OnControllerPromotionClaimed()
{
  if (!controller)
    controller = std::make_unique<Controller>(GetLogger());
  controller->Start();
}

void AtlasNet::AtlasNetNode::TryClaimControllerPromotion()
{
  if (!HasCapability(nodeConfig.capabilities,
                     NodeCapability::ControllerEligible))
    return;

  auto claim = intentRPC->Call<RPC::Database::ClaimControllerPromotion>(
      Network::Intent::Recepient::DatabaseRecepient{}, GetNodeID());
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (claim.wait_for(std::chrono::seconds(0)) !=
             std::future_status::ready &&
         std::chrono::steady_clock::now() < deadline)
  {
    channelBus->TryReceive();
    intentRPC->Poll(Network::PollType::NonBlocking);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (claim.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
  {
    GetLogger()->warn("Timed out claiming Controller promotion over intent RPC");
    return;
  }

  const auto result = claim.get();
  if (!result.has_value() ||
      result.value() ==
          RPC::Database::ClaimControllerPromotionResponse::FAILURE)
    GetLogger()->warn("Failed to claim Controller promotion");
  else if (result.value() ==
           RPC::Database::ClaimControllerPromotionResponse::CLAIMED)
  {
    GetLogger()->info("Claimed Controller promotion over intent RPC");
    OnControllerPromotionClaimed();
  }
  else
    GetLogger()->info("Controller promotion is already claimed");
}

AtlasNet::AtlasNetNode::~AtlasNetNode()
{
  Shutdown();
  for (const auto* prefix :
       {"AtlasNet:", "ChannelBusNetworkTransport:", "HandshakeTransport:",
        "HandshakeRPC:", "ChannelBus:"})
    spdlog::drop(prefix + nodeID.to_string());
}
