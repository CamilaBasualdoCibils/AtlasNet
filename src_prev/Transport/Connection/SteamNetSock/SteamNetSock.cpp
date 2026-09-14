
#include "AtlasNet/Core/Network/Transport/Connection/ConnectionCommons.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionListener.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnection.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionTransport.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSock.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSockConnection.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSockListener.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include <memory>
void AtlasNet::Network::SteamNetSockTransport::OnSteamNetConnectionStatusChanged_s(
    SteamNetConnectionStatusChangedCallback_t* info)
{
  SteamNetSockTransport* transport =
      reinterpret_cast<SteamNetSockTransport*>(info->m_info.m_nUserData);
  assert(transport != nullptr &&
         "Invalid transport pointer in OnSteamNetConnectionStatusChanged_s");
  transport->OnSteamNetConnectionStatusChanged(info);
}
void AtlasNet::Network::SteamNetSockTransport::OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info)
{

  logger->info("OnSteamNetConnectionStatusChanged: {}",
               info->m_info.m_szConnectionDescription);
  switch (info->m_info.m_eState)
  {
  case k_ESteamNetworkingConnectionState_None:
  case k_ESteamNetworkingConnectionState_Connecting:

    OnSteamNetConnectionStatusChanged_Connecting(info);
    break;
  case k_ESteamNetworkingConnectionState_Connected:
    OnSteamNetConnectionStatusChanged_Connected(info);
    break;
  case k_ESteamNetworkingConnectionState_FindingRoute:
  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
  case k_ESteamNetworkingConnectionState_FinWait:
  case k_ESteamNetworkingConnectionState_Linger:
  case k_ESteamNetworkingConnectionState_Dead:
  case k_ESteamNetworkingConnectionState__Force32Bit:
    break;
  }
}
void AtlasNet::Network::SteamNetSockTransport::
    OnSteamNetConnectionStatusChanged_Connecting(
        SteamNetConnectionStatusChangedCallback_t* info)
{
  logger->info("Incoming connection request from {}",
               SocketAddress(info->m_info.m_addrRemote).to_string());
  const bool ReceivedOnListener =
      (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);
  if (ReceivedOnListener)
  {
    std::shared_lock lock(listenersMutex);
    auto it = listeners.find(info->m_info.m_hListenSocket);
    if (it != listeners.end())
    {
      auto listener = it->second;
      if (listener->connectionRequestCallback)
      {
        auto acceptCallback = [&]()
        {
          EResult result =
              SteamNetworkingSockets()->AcceptConnection(info->m_hConn);
          if (result != k_EResultOK)
          {
            logger->error("AcceptConnection failed: {} for {}",
                          static_cast<int>(result),
                          info->m_info.m_szConnectionDescription);
            SteamNetworkingSockets()->CloseConnection(
                info->m_hConn, 0, "AcceptConnection failed", false);
          }
          /* bool pollResult = SteamNetworkingSockets()->SetConnectionPollGroup(
              info->m_hConn, pollGroup);
          if (!pollResult)
          {
            logger->error("Failed to assign accepted connection to poll group");
            SteamNetworkingSockets()->CloseConnection(
                info->m_hConn, 0, "Failed to assign poll group", false);
          } */

          auto connection = std::make_shared<SteamNetSockConnection>(
              this, info->m_hConn, SocketAddress(info->m_info.m_addrRemote));
          auto listener = _GetListener(info->m_info.m_hListenSocket);
          assert(listener != nullptr &&
                 "Listener should exist for incoming connection");
          listener->_InsertServerConnection(info->m_hConn, connection);
          listener->_GetServerConnection(info->m_hConn)
              ->_NotifyStateChange(ConnectionState::Connecting);

          return connection;
        };

        auto rejectCallback = [&]()
        {
          SteamNetworkingSockets()->CloseConnection(
              info->m_hConn, 0, "Connection request rejected", false);
        };
        ConnectionRequest request(listener.get(),
                                  SocketAddress(info->m_info.m_addrRemote),
                                  acceptCallback, rejectCallback);

        if (!listener->connectionRequestCallback)
        {
          logger->warn("No connection request callback set for listener on {}, "
                       "rejecting connection request from {}",
                       listener->remoteAddress.to_string(),
                       info->m_info.m_szConnectionDescription);
          rejectCallback();
        }
        listener->connectionRequestCallback(request, *listener);
        if (!request.WasHandled())
        {
          logger->warn("Connection request from {} was not handled by listener "
                       "on {}, rejecting connection request",
                       info->m_info.m_szConnectionDescription,
                       listener->remoteAddress.to_string());
          rejectCallback();
        }
      }
    }
    else
    {
      assert(false && "Received connection request on unknown listener");
      logger->error("Received connection request on unknown listener: {}",
                    info->m_info.m_szConnectionDescription);
    }
  }
  else
  {
    logger->info("Outgoing connection (initiated locally) to {}",
                 SocketAddress(info->m_info.m_addrRemote).to_string());
    clientConnectionsMutex.lock();
    auto it = clientConnections.find(info->m_hConn);
    if (it != clientConnections.end())
    {
      auto connection = it->second;
      connection->_NotifyStateChange(ConnectionState::Connecting);
    }
    else
    {
      assert(false &&
             "Received connection status change for unknown connection");
      logger->error(
          "Received connection status change for unknown connection: {}",
          info->m_info.m_szConnectionDescription);
    }
    clientConnectionsMutex.unlock();
  }
}
void AtlasNet::Network::SteamNetSockTransport::
    OnSteamNetConnectionStatusChanged_Connected(
        SteamNetConnectionStatusChangedCallback_t* info)
{
  logger->info("Connection established with {}",
               SocketAddress(info->m_info.m_addrRemote).to_string());
  const bool ReceivedOnListener =
      (info->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);
  if (ReceivedOnListener)
  {
    std::shared_lock lock(listenersMutex);
    auto it = listeners.find(info->m_info.m_hListenSocket);
    if (it != listeners.end())
    {
      auto listener = it->second;
      std::shared_lock serverLock(listener->serverConnectionsMutex);
      auto connIt = listener->serverConnections.find(info->m_hConn);
      if (connIt != listener->serverConnections.end())
      {
        auto connection = connIt->second;
        connection->_NotifyStateChange(ConnectionState::Connected);
      }
      else
      {
        assert(false && "Received connection status change for unknown "
                        "connection on listener");
        logger->error(
            "Received connection status change for unknown connection on "
            "listener: {}",
            info->m_info.m_szConnectionDescription);
      }
    }
    else
    {
      assert(false && "Received connection status change on unknown listener");
      logger->error("Received connection status change on unknown listener: {}",
                    info->m_info.m_szConnectionDescription);
    }
  }
  else
  {
    clientConnectionsMutex.lock();
    auto it = clientConnections.find(info->m_hConn);
    if (it != clientConnections.end())
    {
      auto connection = it->second;
      connection->_NotifyStateChange(ConnectionState::Connected);
    }
    else
    {
      assert(false &&
             "Received connection status change for unknown connection");
      logger->error(
          "Received connection status change for unknown connection: {}",
          info->m_info.m_szConnectionDescription);
    }
    clientConnectionsMutex.unlock();
  }
}

void AtlasNet::Network::SteamNetSockTransport::SteamNetSockDebugOutput(
    ESteamNetworkingSocketsDebugOutputType type, const char* pszMsg)
{

  const std::string str = std::format("SteamNetSock Debug: {}", pszMsg);
  switch (type)
  {
  case k_ESteamNetworkingSocketsDebugOutputType_None:
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Bug:
  case k_ESteamNetworkingSocketsDebugOutputType_Error:
    spdlog::error(str);
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Important:
  case k_ESteamNetworkingSocketsDebugOutputType_Warning:
    spdlog::warn(str);
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Msg:
  case k_ESteamNetworkingSocketsDebugOutputType_Verbose:
    spdlog::info(str);
    break;

  case k_ESteamNetworkingSocketsDebugOutputType_Debug:
    spdlog::debug(str);
    break;
  case k_ESteamNetworkingSocketsDebugOutputType_Everything:
  case k_ESteamNetworkingSocketsDebugOutputType__Force32Bit:
    break;
  };
}
void AtlasNet::Network::SteamNetSockTransport::_InsertClientConnection(
    HSteamNetConnection handle,
    std::shared_ptr<SteamNetSockConnection> connection)
{
  std::unique_lock lock(clientConnectionsMutex);
  clientConnections[handle] = connection;
}
void AtlasNet::Network::SteamNetSockTransport::_RemoveClientConnection(
    HSteamNetConnection handle)
{
  std::unique_lock lock(clientConnectionsMutex);
  clientConnections.erase(handle);
}
std::shared_ptr<AtlasNet::Network::SteamNetSockConnection>
AtlasNet::Network::SteamNetSockTransport::_GetClientConnection(
    HSteamNetConnection handle)
{
  std::shared_lock lock(clientConnectionsMutex);
  auto it = clientConnections.find(handle);
  if (it != clientConnections.end())
  {
    return it->second;
  }
  return nullptr;
}
void AtlasNet::Network::SteamNetSockTransport::_InsertListener(
    HSteamListenSocket handle, std::shared_ptr<SteamNetSockListener> listener)
{
  std::unique_lock lock(listenersMutex);
  listeners[handle] = listener;
}
void AtlasNet::Network::SteamNetSockTransport::_RemoveListener(HSteamListenSocket handle)
{
  std::unique_lock lock(listenersMutex);
  listeners.erase(handle);
}
std::shared_ptr<AtlasNet::Network::SteamNetSockListener>
AtlasNet::Network::SteamNetSockTransport::_GetListener(HSteamListenSocket handle)
{
  std::shared_lock lock(listenersMutex);
  auto it = listeners.find(handle);
  if (it != listeners.end())
  {
    return it->second;
  }
  return nullptr;
}

AtlasNet::Network::SteamNetSockTransport::SteamNetSockTransport()
{

  SteamNetworkingErrMsg errMsg = {0};
  const bool result = GameNetworkingSockets_Init(nullptr, errMsg);
  if (errMsg[0] != '\0')
  {
    logger->error("GameNetworkingSockets_Init message: {}", errMsg);
  }
  if (!result)
  {
    throw std::runtime_error(
        std::string("Failed to initialize GameNetworkingSockets: ") + errMsg);
  }
  logger->info("SteamNetworkingSockets initialized");
  SteamNetworkingUtils()->SetDebugOutputFunction(
      k_ESteamNetworkingSocketsDebugOutputType_Msg,
      &SteamNetSockTransport::SteamNetSockDebugOutput);
  /* pollGroup = SteamNetworkingSockets()->CreatePollGroup();
  if (pollGroup == k_HSteamNetPollGroup_Invalid)
  {
    throw std::runtime_error("Failed to create poll group");
  } */
  pollThread = std::jthread(
      [this](std::stop_token stoken)
      {
        while (stoken.stop_requested() == false)
        {
          SteamNetworkingSockets()->RunCallbacks();
          std::this_thread::sleep_for(
              std::chrono::milliseconds(10)); // 10ms = 100 Hz
        }
      });
}
AtlasNet::Network::SteamNetSockTransport::~SteamNetSockTransport() {}

std::shared_ptr<AtlasNet::Network::IConnectionListener>
AtlasNet::Network::SteamNetSockTransport::Listen(const SocketAddress& address)
{
  SteamNetworkingConfigValue_t opts[2];
  opts[0].SetPtr(
      k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
      (void*)&SteamNetSockTransport::OnSteamNetConnectionStatusChanged_s);
  opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, (int64_t)this);

  HSteamListenSocket listenSocket =
      SteamNetworkingSockets()->CreateListenSocketIP(address.ToSteamAddr(), 2,
                                                     opts);
  if (listenSocket == k_HSteamListenSocket_Invalid)
  {
    logger->error("Failed to create listen socket on {}", address.to_string());
    return nullptr;
  }
    logger->info("Created listen socket on {}", address.to_string());
  auto listener =
      std::make_shared<SteamNetSockListener>(this, listenSocket, address);
  _InsertListener(listenSocket, listener);

  return listener;
}

std::shared_ptr<AtlasNet::Network::IConnection>
AtlasNet::Network::SteamNetSockTransport::Connect(const SocketAddress& address)
{
  SteamNetworkingConfigValue_t opts[2];
  opts[0].SetPtr(
      k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
      (void*)&SteamNetSockTransport::OnSteamNetConnectionStatusChanged_s);
  opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, (int64_t)this);

  SteamNetworkingIPAddr steamAddr = address.ToSteamAddr();

  const HSteamNetConnection con =
      SteamNetworkingSockets()->ConnectByIPAddress(steamAddr, 2, opts);
  if (con == k_HSteamNetConnection_Invalid)
  {
    logger->error("Failed to create connection to {}", address.to_string());
    return nullptr;
  }
  auto connection =
      std::make_shared<SteamNetSockConnection>(this, con, address);
  _InsertClientConnection(con, connection);
  return connection;
}
