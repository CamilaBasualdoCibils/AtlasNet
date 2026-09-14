#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnection.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionListener.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionTransport.hpp"
#include "SteamNetSockConnection.hpp"
#include "SteamNetSockListener.hpp"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <memory>
#include <shared_mutex>
#include <thread>
namespace AtlasNet::Network
{

class SteamNetSockTransport : public IConnectionTransport
{
  friend class SteamNetSockConnection;
  friend class SteamNetSockListener;
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("SteamNetSock");
  std::jthread pollThread;
  std::shared_mutex clientConnectionsMutex;

  std::unordered_map<HSteamNetConnection,
                     std::shared_ptr<SteamNetSockConnection>>
      clientConnections;

  std::shared_mutex listenersMutex;
  std::unordered_map<HSteamListenSocket, std::shared_ptr<SteamNetSockListener>>
      listeners;

public:
  SteamNetSockTransport();
  ~SteamNetSockTransport() override;

  std::shared_ptr<IConnectionListener> Listen(const SocketAddress&) override;
  std::shared_ptr<IConnection> Connect(const SocketAddress&) override;

private:
  static void OnSteamNetConnectionStatusChanged_s(
      SteamNetConnectionStatusChangedCallback_t* info);
  void OnSteamNetConnectionStatusChanged(
      SteamNetConnectionStatusChangedCallback_t* info);
  void OnSteamNetConnectionStatusChanged_Connecting(
      SteamNetConnectionStatusChangedCallback_t* info);
  void OnSteamNetConnectionStatusChanged_Connected(
      SteamNetConnectionStatusChangedCallback_t* info);
  static void
  SteamNetSockDebugOutput(ESteamNetworkingSocketsDebugOutputType type,
                          const char* pszMsg);

  void
  _InsertClientConnection(HSteamNetConnection handle,
                          std::shared_ptr<SteamNetSockConnection> connection);
  void _RemoveClientConnection(HSteamNetConnection handle);
  std::shared_ptr<SteamNetSockConnection>
  _GetClientConnection(HSteamNetConnection handle);

  void _InsertListener(HSteamListenSocket handle,
                       std::shared_ptr<SteamNetSockListener> listener);
  void _RemoveListener(HSteamListenSocket handle);
  std::shared_ptr<SteamNetSockListener> _GetListener(HSteamListenSocket handle);

protected:
  auto GetLogger() const
  {
    return logger;
  }
};

} // namespace AtlasNet::Network