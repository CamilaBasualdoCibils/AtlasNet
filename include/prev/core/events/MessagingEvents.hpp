#pragma once

#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/events//IEvent.hpp"

namespace AtlasNet
{
enum class ConnectionSource : uint8_t
{
  Internal = 0,
  External = 1,
  Unverified = 2
};
};
namespace AtlasNet
{
ATLASNET_EVENT(ConnectionStartedInternallyEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));

ATLASNET_EVENT(ConnectionRequestReceivedEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, remoteAddr),
               ATLASNET_EVENT_FIELD(AtlasNet::Network::PortType, localPort));

ATLASNET_EVENT(ConnectionAcceptedInternallyPreHandshakeEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));

ATLASNET_EVENT(ConnectionAcceptedExternalPreHandshakeEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));

ATLASNET_EVENT(ConnectionEstablishedEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address),
               ATLASNET_EVENT_FIELD(AtlasNet::ConnectionSource, source));

ATLASNET_EVENT(HandshakeProcessDeniedEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));

ATLASNET_EVENT(ConnectionTerminatedByRemoteEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));

ATLASNET_EVENT(ConnectionTerminatedByLocalEvent,
               ATLASNET_EVENT_FIELD(AtlasNet::Network::SocketAddress, address));
} // namespace AtlasNet