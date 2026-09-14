#pragma once

#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/V1/ClusterChannelV1.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterDatagram.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/IClusterResolver.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
#include <memory>
#include <spdlog/logger.h>
#include <unordered_map>
namespace AtlasNet::Network::Cluster
{
class ChannelBus
{

  std::shared_ptr<ClusterTransport> m_Transport;
  std::unordered_map<ChannelID,
                     std::pair<std::shared_ptr<ChannelTransportProxy>,
                               std::shared_ptr<IClusterChannel>>>
      m_Channels;
  std::shared_ptr<spdlog::logger> logger_ =
      spdlog::stdout_color_mt("ChannelBus");

public:
  struct ChannelBusOptions
  {
    std::shared_ptr<ClusterTransport> transport;
  };
  ChannelBus(const ChannelBusOptions& options) : m_Transport(options.transport)
  {
  }
  std::shared_ptr<IClusterChannel> MakeChannel(const ChannelOptions& options)
  {
    std::shared_ptr<ChannelTransportProxy> proxy =
        std::make_shared<ChannelTransportProxy>(this, options.id);
    std::shared_ptr<IClusterChannel> channel =
        std::make_shared<ClusterChannelV1>(options, proxy);
    m_Channels[options.id] = std::make_pair(proxy, channel);
    return channel;
  }

  bool SendMessage(ChannelID channelID, const AtlasNetNodeID& destination,
                   std::span<const std::byte> payload)
  {
    return m_Transport->Send(destination, payload);
  }
  void Receive()
  {
    std::array<ClusterDatagram, 128> packets;
    size_t received = m_Transport->Receive(packets);
    Route(std::span<ClusterDatagram>(packets.data(), received));
  }
  void TryReceive()
  {
    std::array<ClusterDatagram, 128> packets;
    size_t received = m_Transport->TryReceive(packets);
    if (received > 0)
    {
      Route(std::span<ClusterDatagram>(packets.data(), received));
    }
  }

private:
  void Route(std::span<ClusterDatagram> packets)
  {
    for (const ClusterDatagram& datagram : packets)
    {

      ChannelV1::PacketHeader header;
      try
      {
        NetBinaryReader reader(datagram.GetPayload());
        header.serialize(reader);
      }
      catch (const std::exception& e)
      {
        // Invalid packet, skip it.
        logger_->warn("Discarding invalid packet from {}: {}, failed to "
                      "deserialize header",
                      datagram.source.to_string(), e.what());
        continue;
      }
      if (header.magic != ChannelV1::CLUSTER_CHANNEL_V1_MAGIC ||
          header.version != ChannelV1::CLUSTER_CHANNEL_V1_VERSION)
      {
        logger_->warn(
            "Discarding invalid packet from {}: invalid magic or version",
            datagram.source.to_string());
        continue;
      }
      auto channelIt = m_Channels.find(header.channel);
      if (channelIt == m_Channels.end())
      {
        logger_->warn("Discarding packet from {}: unknown channel ID {}",
                      datagram.source.to_string(), header.channel);
        continue;
      }
      channelIt->second.first->PushDatagram(datagram);
    }
  }
};

} // namespace AtlasNet::Network::Cluster