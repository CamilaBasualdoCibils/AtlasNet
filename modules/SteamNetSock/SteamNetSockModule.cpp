#include "AtlasNet/Node/Module/ModuleAPI.hpp"
#include <arpa/inet.h>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace
{
uint16_t PickEphemeralPort()
{
  const int socketDescriptor = socket(AF_INET6, SOCK_DGRAM, 0);
  if (socketDescriptor < 0)
    throw std::runtime_error("Failed to allocate an ephemeral UDP socket");
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  if (bind(socketDescriptor, reinterpret_cast<const sockaddr*>(&address),
           sizeof(address)) != 0)
  {
    close(socketDescriptor);
    throw std::runtime_error("Failed to select an ephemeral UDP port");
  }
  socklen_t length = sizeof(address);
  if (getsockname(socketDescriptor, reinterpret_cast<sockaddr*>(&address),
                  &length) != 0)
  {
    close(socketDescriptor);
    throw std::runtime_error("Failed to read the ephemeral UDP port");
  }
  close(socketDescriptor);
  return ntohs(address.sin6_port);
}

class SteamNetSockRuntime
{
public:
  SteamNetSockRuntime()
  {
    SteamNetworkingErrMsg error{};
    if (!GameNetworkingSockets_Init(nullptr, error))
      throw std::runtime_error(
          std::string("GameNetworkingSockets initialization failed: ") + error);
  }

  ~SteamNetSockRuntime()
  {
    GameNetworkingSockets_Kill();
  }

  ISteamNetworkingSockets& Sockets() const
  {
    auto* sockets = SteamNetworkingSockets();
    if (!sockets)
      throw std::runtime_error("GameNetworkingSockets is not initialized");
    return *sockets;
  }
};

class SteamNetSockListener final
    : public AtlasNet::Module::ClientIngressListener
{
public:
  SteamNetSockListener(
      std::shared_ptr<SteamNetSockRuntime> runtime,
      AtlasNet::Module::ClientIngressListenerConfig configuration)
      : runtime(std::move(runtime)), configuration(std::move(configuration))
  {
  }

  ~SteamNetSockListener() override
  {
    Shutdown();
  }

  void Start() override
  {
    if (listenSocket != k_HSteamListenSocket_Invalid)
      throw std::logic_error("SteamNetSock listener is already started");
    if (!configuration.arguments.empty())
      throw std::invalid_argument(
          "SteamNetSock does not support listener arguments: " +
          configuration.arguments);

    SteamNetworkingIPAddr address;
    address.Clear();
    address.m_port =
        configuration.port == 0 ? PickEphemeralPort() : configuration.port;
    SteamNetworkingConfigValue_t callback;
    callback.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                    reinterpret_cast<void*>(OnConnectionStatusChanged));
    listenSocket =
        runtime->Sockets().CreateListenSocketIP(address, 1, &callback);
    if (listenSocket == k_HSteamListenSocket_Invalid)
      throw std::runtime_error(
          "Failed to create SteamNetSock listener on port " +
          std::to_string(configuration.port));
    std::lock_guard lock(listenersMutex);
    listeners.emplace(listenSocket, this);
  }

  void Poll() override
  {
    runtime->Sockets().RunCallbacks();
  }

  void Shutdown() override
  {
    if (listenSocket == k_HSteamListenSocket_Invalid)
      return;
    {
      std::lock_guard lock(listenersMutex);
      listeners.erase(listenSocket);
    }
    for (const auto connection : connections)
      runtime->Sockets().CloseConnection(connection, 0,
                                         "AtlasNet listener shutdown", false);
    connections.clear();
    runtime->Sockets().CloseListenSocket(listenSocket);
    listenSocket = k_HSteamListenSocket_Invalid;
  }

private:
  static void
  OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* event)
  {
    std::lock_guard lock(listenersMutex);
    const auto found = listeners.find(event->m_info.m_hListenSocket);
    if (found == listeners.end())
      return;
    found->second->HandleConnectionStatusChanged(*event);
  }

  void HandleConnectionStatusChanged(
      const SteamNetConnectionStatusChangedCallback_t& event)
  {
    switch (event.m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_Connecting:
      if (runtime->Sockets().AcceptConnection(event.m_hConn) == k_EResultOK)
        connections.insert(event.m_hConn);
      else
        runtime->Sockets().CloseConnection(
            event.m_hConn, 0, "AtlasNet failed to accept connection", false);
      break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
      runtime->Sockets().CloseConnection(event.m_hConn, 0,
                                         "AtlasNet connection closed", false);
      connections.erase(event.m_hConn);
      break;
    default:
      break;
    }
  }

  std::shared_ptr<SteamNetSockRuntime> runtime;
  AtlasNet::Module::ClientIngressListenerConfig configuration;
  HSteamListenSocket listenSocket = k_HSteamListenSocket_Invalid;
  std::unordered_set<HSteamNetConnection> connections;
  static inline std::mutex listenersMutex;
  static inline std::unordered_map<HSteamListenSocket, SteamNetSockListener*>
      listeners;
};

class SteamNetSockProvider final
    : public AtlasNet::Module::ClientIngressTransportProvider
{
public:
  SteamNetSockProvider() : runtime(std::make_shared<SteamNetSockRuntime>()) {}

  std::unique_ptr<AtlasNet::Module::ClientIngressListener> CreateListener(
      const AtlasNet::Module::ClientIngressListenerConfig& config) override
  {
    return std::make_unique<SteamNetSockListener>(runtime, config);
  }

private:
  std::shared_ptr<SteamNetSockRuntime> runtime;
};

void Register(AtlasNet::Module::ModuleRegistry& registry)
{
  registry.RegisterClientIngressTransport(
      "SteamNetSock", std::make_shared<SteamNetSockProvider>());
}

const AtlasNet::Module::ModuleDescriptor descriptor{
    .name = "AtlasNetSteamNetSock",
    .version = "1.0.0",
    .capabilities = AtlasNet::Module::Capability::ClientIngressTransport,
    .Register = Register};
} // namespace

extern "C" const AtlasNet::Module::ModuleDescriptor* AtlasNetGetModule()
{
  return &descriptor;
}
