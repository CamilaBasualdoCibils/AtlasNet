#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelCommons.hpp"
namespace AtlasNet::Network::Cluster
{

class ClusterMessage
{
public:
  ClusterMessage() = default;

  ClusterMessage(
      AtlasNetNodeID source,
      ChannelID channel,
      uint64_t sequence,
      std::shared_ptr<const std::vector<std::byte>> storage,
      size_t offset,
      size_t size)
      : source_(source),
        channel_(channel),
        sequence_(sequence),
        storage_(std::move(storage)),
        offset_(offset),
        size_(size)
  {
  }

  [[nodiscard]] const AtlasNetNodeID& Source() const noexcept
  {
    return source_;
  }

  [[nodiscard]] ChannelID Channel() const noexcept
  {
    return channel_;
  }

  [[nodiscard]] uint64_t Sequence() const noexcept
  {
    return sequence_;
  }

  [[nodiscard]] std::span<const std::byte> Payload() const noexcept
  {
    if (!storage_)
      return {};

    return std::span<const std::byte>(*storage_).subspan(offset_, size_);
  }

  void Release() noexcept
  {
    storage_.reset();
    offset_ = 0;
    size_ = 0;
  }

private:
  AtlasNetNodeID source_{};
  ChannelID channel_ = 0;
  uint64_t sequence_ = 0;

  std::shared_ptr<const std::vector<std::byte>> storage_;
  size_t offset_ = 0;
  size_t size_ = 0;
};
} // namespace AtlasNet::Network::Cluster