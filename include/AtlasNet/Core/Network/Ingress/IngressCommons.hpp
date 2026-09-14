#pragma once
#include <boost/describe.hpp>
namespace AtlasNet::Network::Ingress
{
enum class IngressTransportType
{
  TCP,
  SteamNetSock,
  WebSockets
};
BOOST_DESCRIBE_ENUM(IngressTransportType, TCP, SteamNetSock, WebSockets);
} // namespace AtlasNet::Network::Ingress