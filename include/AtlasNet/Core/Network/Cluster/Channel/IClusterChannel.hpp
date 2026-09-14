#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelCommons.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelTransportProxy.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ClusterMessage.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
namespace AtlasNet::Network::Cluster
{

struct ChannelOptions
{
  ChannelID id = -1;

  DeliveryMode delivery = DeliveryMode::Unreliable;
  OrderingMode ordering = OrderingMode::Unordered;
  BatchMode batching = BatchMode::Automatic;

  size_t maxBatchBytes = 32 * 1024;
  size_t maxQueuedMessages = 256;

  constexpr bool Validate() const noexcept
  {
    if (id <= 0)
      return false;

    if (ordering == OrderingMode::Ordered && delivery != DeliveryMode::Reliable)
    {
      return false;
    }

    if (ordering == OrderingMode::Sequenced &&
        delivery != DeliveryMode::Unreliable)
    {
      return false;
    }

    if (maxBatchBytes == 0 || maxQueuedMessages == 0)
    {
      return false;
    }

    return true;
  }
};
class IClusterChannel
{
  const ChannelOptions options;
  std::shared_ptr<ChannelTransportProxy> transport;
public:
  IClusterChannel(const ChannelOptions& options,
                  std::shared_ptr<ChannelTransportProxy> transport)
      : options(options), transport(transport)
  {
    if (!options.Validate())
    {
      throw std::invalid_argument("Invalid channel options");
    }
  }
  const ChannelOptions& GetOptions() const noexcept
  {
    return options;
  }
  ChannelTransportProxy* GetTransport() const noexcept
  {
    return transport.get();
  }
  virtual ~IClusterChannel() = default;

  virtual bool Send(const AtlasNetNodeID& destination,
                    std::span<const std::byte> payload) = 0;

  // Sends all currently queued messages as one or more datagrams.
  virtual void Flush() = 0;

  // Blocking.
  virtual size_t Receive(std::span<ClusterMessage> messages) = 0;

  // Non-blocking.
  virtual size_t TryReceive(std::span<ClusterMessage> messages) = 0;

  virtual void Tick() = 0;
};
} // namespace AtlasNet::Network::Cluster