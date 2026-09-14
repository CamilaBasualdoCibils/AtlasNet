#pragma once

#include "AtlasNet/Core/Network/Cluster/Channel/ClusterMessage.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterDatagram.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace AtlasNet::Network::Cluster
{
namespace ChannelV1
{
constexpr uint32_t CLUSTER_CHANNEL_V1_MAGIC = 0x43484E31; // "CHN1"
constexpr uint8_t CLUSTER_CHANNEL_V1_VERSION = 1;
enum class PacketFlags : uint8_t
{
  None = 0,
  Reliable = 1 << 0,
  AckOnly = 1 << 1,
};
constexpr PacketFlags operator|(PacketFlags lhs, PacketFlags rhs) noexcept
{
  return static_cast<PacketFlags>(static_cast<uint8_t>(lhs) |
                                  static_cast<uint8_t>(rhs));
}

constexpr bool HasFlag(PacketFlags value, PacketFlags flag) noexcept
{
  return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}
// for the packet
struct PacketHeader
{
  uint32_t magic = CLUSTER_CHANNEL_V1_MAGIC;
  uint8_t version = 1;
  PacketFlags flags = PacketFlags::None;
  ChannelID channel = 0;
  uint64_t packetSequence = 0;
  uint64_t ackSequence = 0;
  uint64_t ackBits = 0;
  uint64_t messageCount = 0;
  uint16_t payloadBytes = 0;
  template <typename Archive> void serialize(Archive& archive)
  {
    archive(magic, version, flags, channel, packetSequence, ackSequence,
            ackBits, messageCount, payloadBytes);
  }
  constexpr static size_t NetSize() noexcept
  {
    NetBinaryWriter writer;
    writer(PacketHeader());
    return writer.GetBytes().size();
  }
};
// for each message
struct MessageHeader
{
  uint64_t messageSequence = 0;
  uint16_t payloadBytes = 0;
  template <typename Archive> void serialize(Archive& archive)
  {
    archive(messageSequence, payloadBytes);
  }
  constexpr static size_t NetSize() noexcept
  {
    NetBinaryWriter writer;
    writer(MessageHeader());
    return writer.GetBytes().size();
  }
};

} // namespace ChannelV1
class ClusterChannelV1 : public IClusterChannel
{
public:
  ClusterChannelV1(const ChannelOptions& options,
                   std::shared_ptr<ChannelTransportProxy> transport)
      : IClusterChannel(options, transport),
        logger_(spdlog::stdout_color_mt(std::format("ClusterChannelV1-{}", options.id)))
  {
    logger_->set_level(spdlog::level::trace);
  }

  bool Send(const AtlasNetNodeID& destination,
            std::span<const std::byte> payload) override;

  void Flush() override;

  size_t Receive(std::span<ClusterMessage> messages) override;

  size_t TryReceive(std::span<ClusterMessage> messages) override;
  void Tick() override;

private:
  using Clock = std::chrono::steady_clock;

  struct QueuedMessage
  {
    uint64_t sequence = 0;
    std::vector<std::byte> payload;
  };

  struct PendingPacket
  {
    uint64_t packetSequence = 0;
    std::vector<std::byte> bytes;

    Clock::time_point lastSent;
    uint32_t attempts = 0;
  };

  struct SendState
  {
    uint64_t nextPacketSequence = 1;
    uint64_t nextMessageSequence = 1;

    size_t queuedBytes = 0;
    std::deque<QueuedMessage> queuedMessages;

    // Packets waiting for an ACK.
    std::map<uint64_t, PendingPacket> pendingPackets;
  };

  struct BufferedMessage
  {
    uint64_t sequence = 0;
    std::shared_ptr<const std::vector<std::byte>> storage;
    size_t offset = 0;
    size_t size = 0;
  };

  struct ReceiveState
  {
    // Packet-level ACK state.
    uint64_t highestPacketSequence = 0;
    uint64_t receivedPacketBits = 0;

    // Message-level ordering state.
    uint64_t nextOrderedSequence = 1;
    uint64_t highestSequencedMessage = 0;

    std::map<uint64_t, BufferedMessage> reorderBuffer;

    bool ackPending = false;
  };

  struct PeerState
  {
    SendState send;
    ReceiveState receive;
  };

  struct ReadyMessage
  {
    AtlasNetNodeID source;
    uint64_t sequence = 0;

    std::shared_ptr<const std::vector<std::byte>> storage;
    size_t offset = 0;
    size_t size = 0;
  };

  std::unordered_map<AtlasNetNodeID, PeerState> peers_;
  std::deque<ReadyMessage> readyMessages_;

  std::mutex mutex_;
  std::shared_ptr<spdlog::logger> logger_;

  static constexpr auto RetransmissionTimeout = std::chrono::milliseconds(1000);

  static constexpr uint32_t MaxRetransmissions = 10;

  bool FlushPeer(const AtlasNetNodeID& destination, PeerState& peer);
  void FlushPendingAcks();
  void ProcessTimers();

  size_t PumpTransport(bool blocking);
  void ProcessDatagram(const ClusterDatagram& datagram);

  void ProcessAcknowledgements(SendState& state, uint64_t ackSequence,
                               uint64_t ackBits);

  bool RegisterReceivedPacket(ReceiveState& state, uint64_t packetSequence);

  void ProcessMessage(const AtlasNetNodeID& source, ReceiveState& state,
                      uint64_t messageSequence,
                      std::shared_ptr<const std::vector<std::byte>> storage,
                      size_t offset, size_t size);

  void DeliverOrderedMessages(const AtlasNetNodeID& source,
                              ReceiveState& state);

  size_t CopyReadyMessages(std::span<ClusterMessage> output);

  std::vector<std::byte> BuildPacket(PeerState& peer,
                                     std::span<const QueuedMessage> messages,
                                     uint64_t packetSequence, bool ackOnly);

  void SendPacket(const AtlasNetNodeID& destination, PeerState& peer,
                  std::vector<std::byte> bytes, uint64_t packetSequence);
};
} // namespace AtlasNet::Network::Cluster