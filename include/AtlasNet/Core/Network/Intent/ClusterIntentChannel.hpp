#pragma once

#include "AtlasNet/Core/Network/Cluster/Channel/ClusterMessage.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/Intent/IIntentResolver.hpp"
#include "AtlasNet/Core/Network/Intent/IntentCommons.hpp"
#include "AtlasNet/Core/Network/Intent/IntentRecepient.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <boost/describe/enum_to_string.hpp>
#include <functional>
namespace AtlasNet::Network::Intent
{

constexpr uint8_t DEFAULT_INTENT_TTL = 16;
class IntentDatagram
{
public:
  IntentDatagram() = default;

  IntentDatagram(IntentHeader header, std::span<const std::byte> payload,
                 Cluster::ClusterMessage datagram)
      : payload(payload), header(std::move(header)),
        datagram_(std::move(datagram))
  {
  }

  IntentDatagram(const IntentDatagram&) = delete;
  IntentDatagram& operator=(const IntentDatagram&) = delete;

  IntentDatagram(IntentDatagram&& other) noexcept
      : payload(other.payload), header(std::move(other.header)),
        datagram_(std::move(other.datagram_))
  {
    other.payload = {};
  }

  IntentDatagram& operator=(IntentDatagram&& other) noexcept
  {
    if (this == &other)
      return *this;

    Release();

    payload = other.payload;
    header = std::move(other.header);
    datagram_ = std::move(other.datagram_);

    other.payload = {};

    return *this;
  }

  ~IntentDatagram()
  {
    Release();
  }

  [[nodiscard]]
  std::span<const std::byte> Payload() const
  {
    return payload;
  }

  [[nodiscard]]
  const IntentHeader& Header() const
  {
    return header;
  }
  void Release()
  {
    datagram_.Release();
  }

private:
  Cluster::ClusterMessage datagram_;
  std::span<const std::byte> payload;
  IntentHeader header;
};

class ClusterIntentChannel
{
public:
  ClusterIntentChannel(AtlasNetNodeID selfID,
                       std::shared_ptr<Cluster::IClusterChannel> channel,
                       std::shared_ptr<IIntentResolver> resolver)
      : selfID(selfID), channel(channel), resolver(resolver),
        logger_(spdlog::stdout_color_mt(
            std::format("ClusterIntentChannel-{}", channel->GetOptions().id)))
  {
    if (!channel)
      throw std::invalid_argument("Channel cannot be null");
    if (!resolver)
      throw std::invalid_argument("Resolver cannot be null");
  }
  virtual ~ClusterIntentChannel() = default;

  virtual bool Send(const VIntent& intent, std::span<const std::byte> payload,
                    bool reliable = false)
  {
    const auto nodeID = resolver->ResolveIntent(intent);
    if (!nodeID)
    {
      logger_->warn(
          "Failed to resolve intent {} to a node (reliable={})",
          boost::describe::enum_to_string(
              std::visit([](auto&& arg) { return arg.Method; }, intent),
              "INVALID"),
          reliable);
      return false;
    }
    logger_->trace(
        "Sending {} bytes with intent {} to node {} (reliable={})",
        payload.size(),
        boost::describe::enum_to_string(
            std::visit([](auto&& arg) { return arg.Method; }, intent),
            "INVALID"),
        nodeID->to_string(), reliable);
    if (!nodeID)
      return false;

    IntentHeader header{
        .source = selfID,
        .ttl = DEFAULT_INTENT_TTL,
        .correlationID = NextCorrelationID(),
        .method = GetIntentMethod(intent),
        .ackMode = reliable ? IntentAckMode::Request : IntentAckMode::None,
        .intent = intent,
        .payloadBytes = static_cast<uint16_t>(payload.size()),
    };

    auto packet = BuildPacket(header, payload);
    if (!channel->Send(*nodeID, packet))
      return false;

    if (reliable)
      RegisterPendingAck(header.correlationID, *nodeID, packet);

    return true;
  }

  virtual size_t Receive(std::span<IntentDatagram> messages)
  {
    return ReceiveImpl(messages, false);
  }

  virtual size_t TryReceive(std::span<IntentDatagram> messages)
  {
    return ReceiveImpl(messages, true);
  }
  virtual void Tick()
  {
    channel->Tick();
  }

private:
  struct ParsedPacket
  {
    IntentHeader header;
    std::span<const std::byte> payload;
  };

  struct PendingReliableMessage
  {
    AtlasNetNodeID lastDestination;
    std::vector<std::byte> packet;
  };

  size_t ReceiveImpl(std::span<IntentDatagram> messages, bool nonBlocking)
  {
    if (messages.empty())
      return 0;

    /*
     * We intentionally request more underlying datagrams than the caller
     * requested because some packets may disappear internally due to:
     *
     *   - forwarding
     *   - ACK processing
     *   - expired TTL
     *   - invalid intent
     */
    std::array<Cluster::ClusterMessage, 64> incoming;

    size_t outputCount = 0;

    while (outputCount < messages.size())
    {
      const size_t count = nonBlocking ? channel->TryReceive(incoming)
                                       : channel->Receive(incoming);

      if (count == 0)
        break;

      for (size_t i = 0; i < count && outputCount < messages.size(); ++i)
      {
        auto& datagram = incoming[i];

        const auto parsed = ParsePacket(datagram.Payload());

        if (!parsed)
        {
          logger_->warn("Discarding invalid packet from {}",
                        datagram.Source().to_string());
          datagram.Release();
          continue;
        }

        IntentHeader header = parsed->header;

        /*
         * ACKs are channel-control traffic.
         * They are never exposed to the user.
         */
        if (header.ackMode == IntentAckMode::Response)
        {
          logger_->trace("Received ACK for correlationID {} from {}",
                         header.correlationID, datagram.Source().to_string());
          HandleAck(header);
          datagram.Release();
          continue;
        }

        /*
         * Work out who CURRENTLY owns this intent.
         *
         * This is deliberately done again at every hop.
         */
        const auto currentOwner = resolver->ResolveIntent(header.intent);

        if (!currentOwner)
        {
          logger_->warn(
              "Discarding intent {} from {}: failed to resolve current owner",
              boost::describe::enum_to_string(header.method, "INVALID"),
              datagram.Source().to_string());
          datagram.Release();
          continue;
        }

        /*
         * Intent is stale for this node.
         *
         * Forward it without touching:
         *
         *   source
         *   correlationID
         *   ackMode
         *   intent
         *
         * Only TTL changes.
         */
        if (*currentOwner != selfID)
        {
          logger_->trace(
              "Forwarding intent {} from {} to {} (reliable={})",
              boost::describe::enum_to_string(header.method, "INVALID"),
              datagram.Source().to_string(), currentOwner->to_string(),
              header.ackMode == IntentAckMode::Request);
          if (header.ttl == 0)
          {
            logger_->warn(
                "Discarding intent {} from {}: TTL expired, cannot forward to "
                "{}",
                boost::describe::enum_to_string(header.method, "INVALID"),
                datagram.Source().to_string(), currentOwner->to_string());
            datagram.Release();
            continue;
          }

          --header.ttl;

          auto forwarded = BuildPacket(header, parsed->payload);

          channel->Send(*currentOwner, forwarded);

          datagram.Release();
          continue;
        }

        /*
         * We are the CURRENT valid destination.
         *
         * This is the only place where an ACK may be generated.
         */
        if (header.ackMode == IntentAckMode::Request)
        {
          logger_->trace("Sending ACK for correlationID {} to {}",
                         header.correlationID, datagram.Source().to_string());
          SendAck(header);
        }

        /*
         * The IntentDatagram borrows the underlying cluster datagram's
         * payload memory. Therefore transfer ownership of that datagram's
         * lifetime into IntentDatagram.
         *
         * Replace these APIs with the exact ownership mechanism used by
         * your ClusterMessage.
         */
        IntentDatagram intentDatagram{
            std::move(header), parsed->payload.subspan(0, header.payloadBytes),
            datagram};

        messages[outputCount++] = std::move(intentDatagram);
      }

      /*
       * For TryReceive(), drain currently available packets, but don't
       * turn it into a blocking call while searching for application
       * messages.
       */
      if (nonBlocking)
        continue;

      /*
       * Receive() may have consumed a complete batch consisting only of
       * redirects/ACKs, so continue until we either obtain an application
       * message or the underlying channel returns nothing.
       */
      if (outputCount > 0)
        break;
    }
    if (outputCount > 0)
    {
      logger_->trace("Received {} intent messages", outputCount);
    }

    return outputCount;
  }

  std::optional<ParsedPacket> ParsePacket(std::span<const std::byte> packet)
  {
    NetBinaryReader reader(packet);

    IntentHeader header;
    reader(header);

    if (!header.IsValid())
      return std::nullopt;

    const size_t offset = reader->adapter().currentReadPos();

    if (offset > packet.size())
      return std::nullopt;

    return ParsedPacket{
        .header = std::move(header),
        .payload = packet.subspan(offset, offset + header.payloadBytes),
    };
  }

  void SendAck(const IntentHeader& received)
  {
    IntentHeader ack{
        /*
         * ACK physically originates here.
         */
        .source = selfID,

        /*
         * ACK is direct node-to-node control traffic; it isn't rerouted
         * through the intent.
         */
        .ttl = 0,

        /*
         * Critical: identify the original reliable message.
         */
        .correlationID = received.correlationID,
        .method = received.method,

        .ackMode = IntentAckMode::Response,

        /*
         * Keeping the original intent can be useful for diagnostics,
         * though it is not necessary for ACK routing.
         */
        .intent = received.intent,
    };

    auto packet = BuildPacket(ack, {});

    /*
     * Most important line:
     *
     * ACK goes to ORIGINAL SOURCE, not the forwarding node.
     */
    channel->Send(received.source, packet);
  }

  void HandleAck(const IntentHeader& ack)
  {
    /*
     * Ignore ACKs that aren't for something originated by us.
     */
    if (ack.source == selfID)
      return;

    pendingReliable.erase(ack.correlationID);
  }

  void RegisterPendingAck(uint64_t correlationID, AtlasNetNodeID destination,
                          std::vector<std::byte> packet)
  {
    pendingReliable.insert_or_assign(correlationID,
                                     PendingReliableMessage{
                                         .lastDestination = destination,
                                         .packet = std::move(packet),
                                     });
  }

  uint64_t NextCorrelationID()
  {
    /*
     * If ClusterIntentChannel is accessed concurrently, make this atomic.
     */
    return nextCorrelationID.fetch_add(1, std::memory_order_relaxed);
  }

  IntentMethod GetIntentMethod(const VIntent& intent) const
  {
    return std::visit([](const auto& value) -> IntentMethod
                      { return value.Method; }, intent);
  }

  std::vector<std::byte> BuildPacket(const IntentHeader& header,
                                     std::span<const std::byte> payload)
  {
    assert(!std::holds_alternative<Recepient::InvalidRecepient>(header.intent));

    NetBinaryWriter writer;

    writer(header);

    writer->adapter().writeBuffer<sizeof(uint8_t)>(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    return writer.Release();
  }

private:
  AtlasNetNodeID selfID;

  std::shared_ptr<Cluster::IClusterChannel> channel;
  std::shared_ptr<IIntentResolver> resolver;

  std::atomic<uint64_t> nextCorrelationID{1};

  std::unordered_map<uint64_t, PendingReliableMessage> pendingReliable;
  std::shared_ptr<spdlog::logger> logger_;
};
} // namespace AtlasNet::Network::Intent