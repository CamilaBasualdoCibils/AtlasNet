#pragma once

#include "AtlasNet/Core/Network/Cluster/Channel/ChannelCommons.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
#include <queue>
namespace AtlasNet::Network::Cluster
{
class ChannelBus;

class ChannelTransportProxy
{
  std::queue<ClusterDatagram> m_ReceiveQueue;
  std::mutex m_ReceiveMutex;
  std::condition_variable m_ReceiveCondition;
  ChannelBus* m_ChannelBus;
  const ChannelID m_ChannelID;

public:
  virtual void PushDatagram(ClusterDatagram datagram)
  {
    {
      std::scoped_lock lock(m_ReceiveMutex);

      m_ReceiveQueue.push(std::move(datagram));
    }

    m_ReceiveCondition.notify_one();
  }
  ChannelTransportProxy(ChannelBus* channelBus, ChannelID channelID)
      : m_ChannelBus(channelBus), m_ChannelID(channelID)
  {
  }
  virtual bool Send(const AtlasNetNodeID& destination,
                           std::span<const std::byte> payload);

  virtual size_t Receive(std::span<ClusterDatagram> packets)
  {
    assert(packets.size() > 0);
    if (packets.empty())
      return 0;

    std::unique_lock lock(m_ReceiveMutex);

    m_ReceiveCondition.wait(lock, [this] { return !m_ReceiveQueue.empty(); });

    size_t received = 0;

    while (received < packets.size() && !m_ReceiveQueue.empty())
    {
      packets[received] = std::move(m_ReceiveQueue.front());

      m_ReceiveQueue.pop();

      ++received;
    }

    return received;
  }

  virtual size_t TryReceive(std::span<ClusterDatagram> packets)
  {
    assert(packets.size() > 0);
    if (packets.empty())
      return 0;

    std::scoped_lock lock(m_ReceiveMutex);

    size_t received = 0;

    while (received < packets.size() && !m_ReceiveQueue.empty())
    {
      packets[received] = std::move(m_ReceiveQueue.front());

      m_ReceiveQueue.pop();

      ++received;
    }

    return received;
  }
};
} // namespace AtlasNet::Network::Cluster