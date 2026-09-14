#pragma once

#include "AtlasNet/Core/Network/Transport/Connection/IConnectionListener.hpp"
#include <mutex>
#include <shared_mutex>
namespace AtlasNet::Network
{
class SteamNetSockTransport;
class SteamNetSockListener;
class SteamNetSockConnection;
class SteamNetSockListener : public IConnectionListener
{
  friend class SteamNetSockTransport;
  SteamNetSockTransport* steamNetworkingSockets;
  HSteamListenSocket listenSocket;
  SocketAddress remoteAddress;
  std::function<void(ConnectionRequest&, IConnectionListener&)> connectionRequestCallback;

  std::shared_mutex serverConnectionsMutex;
  std::unordered_map<HSteamNetConnection,
                     std::shared_ptr<SteamNetSockConnection>>
      serverConnections;

public:
  void SetConnectionRequestCallback(
      std::function<void(ConnectionRequest&, IConnectionListener&)> callback) override;

  void Close() override;

  SteamNetSockListener(SteamNetSockTransport* transport,
                       HSteamListenSocket socket, const SocketAddress& address);

private:
  auto GetLogger() const;

protected:
  void
  _InsertServerConnection(HSteamNetConnection handle,
                          std::shared_ptr<SteamNetSockConnection> connection)
  {
    std::unique_lock lock(serverConnectionsMutex);
    serverConnections[handle] = connection;
  }
  void _RemoveServerConnection(HSteamNetConnection handle)
  {
    std::unique_lock lock(serverConnectionsMutex);
    serverConnections.erase(handle);
  }
  std::shared_ptr<SteamNetSockConnection>
  _GetServerConnection(HSteamNetConnection handle)
  {
    std::shared_lock lock(serverConnectionsMutex);
    auto it = serverConnections.find(handle);
    if (it != serverConnections.end())
    {
      return it->second;
    }
    return nullptr;
  }
};

}; // namespace AtlasNet