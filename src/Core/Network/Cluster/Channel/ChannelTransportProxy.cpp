#include "AtlasNet/Core/Network/Cluster/Channel/ChannelTransportProxy.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelBus.hpp"
bool AtlasNet::Network::Cluster::ChannelTransportProxy::Send(
    const AtlasNetNodeID& destination, std::span<const std::byte> payload)
{
  return m_ChannelBus->SendMessage(m_ChannelID, destination, payload);
}