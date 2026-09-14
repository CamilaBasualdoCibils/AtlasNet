#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSockListener.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnectionListener.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSock.hpp"
void AtlasNet::Network::SteamNetSockListener::SetConnectionRequestCallback(
    std::function<void(ConnectionRequest&, IConnectionListener&)> callback)
{
  connectionRequestCallback = callback;
}
void AtlasNet::Network::SteamNetSockListener::Close()
{
  // TODO: Implement this pure virtual method.
  /* static_assert(false, "Method `Close` is not implemented."); */
}
AtlasNet::Network::SteamNetSockListener::SteamNetSockListener(
    SteamNetSockTransport* transport, HSteamListenSocket socket,
    const SocketAddress& address)
    : steamNetworkingSockets(transport), listenSocket(socket),
      remoteAddress(address)
{
}
auto AtlasNet::Network::SteamNetSockListener::GetLogger() const
{
  return steamNetworkingSockets->GetLogger();
}