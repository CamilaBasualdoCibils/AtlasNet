#include "AtlasNet/Core/Network/Cluster/Channel/V1/ClusterChannelV1.hpp"
#include "AtlasNet/Core/Core.hpp"

bool AtlasNet::Network::Cluster::ClusterChannelV1::Send(
    const AtlasNetNodeID& destination, std::span<const std::byte> payload)
{
  if (payload.empty())
  {
    logger_->warn("Attempted to send empty payload");
    assert(false && "Cannot send empty payload");
    return false;
  }

  const ChannelOptions& options = GetOptions();

  // Account for the per-message framing too.
  const size_t framedSize =
      ChannelV1::MessageHeader::NetSize() + payload.size();

  if (framedSize > options.maxBatchBytes)
  {
    logger_->warn("Message ({} bytes) exceeds max batch size ({})", framedSize,
                  options.maxBatchBytes);
    assert(false && "Message too large for channel batch size");
    // Fragmentation would be required here.
    return false;
  }

  std::scoped_lock lock(mutex_);

  PeerState& peer = peers_[destination];

  QueuedMessage message;

  if (options.ordering != OrderingMode::Unordered ||
      options.delivery == DeliveryMode::Reliable)
  {
    message.sequence = peer.send.nextMessageSequence++;
  }

  message.payload.assign(payload.begin(), payload.end());

  peer.send.queuedBytes += framedSize;
  peer.send.queuedMessages.push_back(std::move(message));
  logger_->trace("Queued message for {} ({} bytes, seq={})",
                 destination.to_string(), payload.size(), message.sequence);

  switch (options.batching)
  {
  case BatchMode::Immediate:
    return FlushPeer(destination, peer);

  case BatchMode::Automatic:
    if (peer.send.queuedBytes >= options.maxBatchBytes ||
        peer.send.queuedMessages.size() >= options.maxQueuedMessages)
    {
      return FlushPeer(destination, peer);
    }

    return true;

  case BatchMode::Manual:
    return true;
  }

  return false;
}
void AtlasNet::Network::Cluster::ClusterChannelV1::Flush()
{
  std::scoped_lock lock(mutex_);

  logger_->trace("Flushing {} peers", peers_.size());
  for (auto& [destination, peer] : peers_)
    FlushPeer(destination, peer);
}
size_t AtlasNet::Network::Cluster::ClusterChannelV1::Receive(
    std::span<ClusterMessage> messages)
{
  if (messages.empty())
    return 0;

  for (;;)
  {
    {
      std::scoped_lock lock(mutex_);

      const size_t count = CopyReadyMessages(messages);

      if (count != 0)
        return count;
    }

    // Do not hold mutex_ while blocking inside the transport.
    PumpTransport(true);
  }
}
size_t AtlasNet::Network::Cluster::ClusterChannelV1::TryReceive(
    std::span<ClusterMessage> messages)
{
  if (messages.empty())
    return 0;

  {
    std::scoped_lock lock(mutex_);

    const size_t ready = CopyReadyMessages(messages);

    if (ready != 0)
      return ready;
  }
  PumpTransport(false);

  {
    std::scoped_lock lock(mutex_);

    return CopyReadyMessages(messages);
  }
}
void AtlasNet::Network::Cluster::ClusterChannelV1::Tick()
{

  PumpTransport(false);
  std::scoped_lock lock(mutex_);

  ProcessTimers();
  FlushPendingAcks();
}

bool AtlasNet::Network::Cluster::ClusterChannelV1::FlushPeer(
    const AtlasNetNodeID& destination, PeerState& peer)
{

  auto& queue = peer.send.queuedMessages;

  if (queue.empty())
    return true;
  logger_->trace("Flushing peer {} ({} queued messages)",
                 destination.to_string(), queue.size());
  bool success = true;

  while (!queue.empty())
  {
    std::vector<QueuedMessage> packetMessages;
    size_t packetBytes = ChannelV1::PacketHeader ::NetSize();

    while (!queue.empty())
    {
      const QueuedMessage& next = queue.front();

      const size_t framedSize =
          ChannelV1::MessageHeader::NetSize() + next.payload.size();

      if (!packetMessages.empty() &&
          packetBytes + framedSize > GetOptions().maxBatchBytes)
      {
        break;
      }

      if (packetBytes + framedSize > GetOptions().maxBatchBytes)
      {
        // This message requires fragmentation.
        success = false;
        queue.pop_front();
        continue;
      }

      packetBytes += framedSize;
      packetMessages.push_back(std::move(queue.front()));
      queue.pop_front();
    }

    if (packetMessages.empty())
    {
      logger_->trace("Peer {} queue empty", destination.to_string());
      continue;
    }

    uint64_t packetSequence = 0;

    if (GetOptions().delivery == DeliveryMode::Reliable)
      packetSequence = peer.send.nextPacketSequence++;

    std::vector<std::byte> bytes =
        BuildPacket(peer, packetMessages, packetSequence, false);

    for (const QueuedMessage& message : packetMessages)
    {
      peer.send.queuedBytes -=
          ChannelV1::MessageHeader::NetSize() + message.payload.size();
    }
    logger_->trace(
        "Sending packet {} -> {} (packetSeq={}, messages={}, bytes={})",
        GetOptions().id, destination.to_string(), packetSequence,
        packetMessages.size(), bytes.size());
    SendPacket(destination, peer, std::move(bytes), packetSequence);
  }

  return success;
}
void AtlasNet::Network::Cluster::ClusterChannelV1::FlushPendingAcks()
{
  for (auto& [destination, peer] : peers_)
  {
    if (!peer.receive.ackPending)
      continue;
    logger_->trace("Sending ACK-only packet to {} (ack={}, bits={:#018x})",
                   destination.to_string(), peer.receive.highestPacketSequence,
                   peer.receive.receivedPacketBits);
    std::vector<std::byte> bytes = BuildPacket(peer, {}, 0, true);

    GetTransport()->Send(destination, bytes);
  }
}
void AtlasNet::Network::Cluster::ClusterChannelV1::ProcessTimers()
{
  if (GetOptions().delivery != DeliveryMode::Reliable)
    return;

  const Clock::time_point now = Clock::now();

  for (auto& [destination, peer] : peers_)
  {
    auto& pendingPackets = peer.send.pendingPackets;

    for (auto iterator = pendingPackets.begin();
         iterator != pendingPackets.end();)
    {
      PendingPacket& packet = iterator->second;

      if (now - packet.lastSent < RetransmissionTimeout)
      {
        ++iterator;
        continue;
      }

      if (packet.attempts >= MaxRetransmissions)
      {
        // Report peer/channel failure through an event or callback.
        iterator = pendingPackets.erase(iterator);
        logger_->error("Packet {} to {} exceeded retransmission limit",
                       packet.packetSequence, destination.to_string());
        continue;
      }
      logger_->trace("Retransmitting packet {} to {} (attempt {}/{})",
                     packet.packetSequence, destination.to_string(),
                     packet.attempts + 1, MaxRetransmissions);
      GetTransport()->Send(destination, packet.bytes);

      packet.lastSent = now;
      ++packet.attempts;
      ++iterator;
    }
  }
}
size_t
AtlasNet::Network::Cluster::ClusterChannelV1::PumpTransport(bool blocking)
{
  constexpr size_t MaxDatagramsPerPump = 128;

  std::array<ClusterDatagram, MaxDatagramsPerPump> packets;

  const size_t received = blocking ? GetTransport()->Receive(packets)
                                   : GetTransport()->TryReceive(packets);

  if (received == 0)
    return 0;
  logger_->trace("Received {} datagrams", received);
  std::scoped_lock lock(mutex_);

  for (size_t index = 0; index < received; ++index)
  {
    ProcessDatagram(packets[index]);
    packets[index].Release();
  }

  return received;
}
void AtlasNet::Network::Cluster::ClusterChannelV1::ProcessDatagram(
    const ClusterDatagram& datagram)
{
  auto storage = std::make_shared<std::vector<std::byte>>(
      datagram.GetPayload().begin(), datagram.GetPayload().end());

  NetBinaryReader reader(*storage);

  ChannelV1::PacketHeader header;
  header.serialize(reader);

  if (header.magic != ChannelV1::CLUSTER_CHANNEL_V1_MAGIC ||
      header.version != ChannelV1::CLUSTER_CHANNEL_V1_VERSION ||
      header.channel != GetOptions().id)
  {
    logger_->warn("Discarding invalid packet from {}",
                  datagram.source.to_string());
    return;
  }

  PeerState& peer = peers_[datagram.source];

  ProcessAcknowledgements(peer.send, header.ackSequence, header.ackBits);

  const bool reliable =
      ChannelV1::HasFlag(header.flags, ChannelV1::PacketFlags::Reliable);
  logger_->trace(
      "Received packet from {} "
      "(packetSeq={}, ack={}, ackBits={:#018x}, messages={}, reliable={})",
      datagram.source.to_string(), header.packetSequence, header.ackSequence,
      header.ackBits, header.messageCount, reliable);
  if (reliable)
  {
    const bool isNew =
        RegisterReceivedPacket(peer.receive, header.packetSequence);

    if (!isNew)
    {
      // Still send/piggyback an ACK, but do not redeliver.
      logger_->trace("Ignoring duplicate packet {} from {}",
                     header.packetSequence, datagram.source.to_string());
      return;
    }
  }

  if (ChannelV1::HasFlag(header.flags, ChannelV1::PacketFlags::AckOnly))
  {
    logger_->trace("Received ACK-only packet from {}",
                   datagram.source.to_string());
    return;
  }

  if (header.messageCount > GetOptions().maxQueuedMessages)
    return;

  for (uint16_t index = 0; index < header.messageCount; ++index)
  {
    ChannelV1::MessageHeader messageHeader;
    messageHeader.serialize(reader);

    if (messageHeader.payloadBytes > reader.Remaining())
      return;

    const size_t payloadOffset = reader.Position();

    ProcessMessage(datagram.source, peer.receive, messageHeader.messageSequence,
                   storage, payloadOffset, messageHeader.payloadBytes);

    reader.Skip(messageHeader.payloadBytes);
  }
}
void AtlasNet::Network::Cluster::ClusterChannelV1::ProcessAcknowledgements(
    SendState& state, uint64_t ackSequence, uint64_t ackBits)
{
  if (ackSequence == 0)
    return;
  logger_->trace("ACK received for packet {}", ackSequence);
  state.pendingPackets.erase(ackSequence);

  for (uint64_t index = 0; index < 64; ++index)
  {
    if ((ackBits & (uint64_t{1} << index)) == 0)
      continue;

    if (ackSequence <= index + 1)
      continue;

    const uint64_t acknowledged = ackSequence - index - 1;
    logger_->trace("ACK bitmap confirmed packet {}", acknowledged);
    state.pendingPackets.erase(acknowledged);
  }
}
bool AtlasNet::Network::Cluster::ClusterChannelV1::RegisterReceivedPacket(
    ReceiveState& state, uint64_t packetSequence)
{
  if (packetSequence == 0)
    return true;

  if (state.highestPacketSequence == 0)
  {
    state.highestPacketSequence = packetSequence;
    state.receivedPacketBits = 0;
    state.ackPending = true;
    return true;
  }

  if (packetSequence > state.highestPacketSequence)
  {

    const uint64_t difference = packetSequence - state.highestPacketSequence;
    logger_->trace("Advanced receive window {} -> {} (delta={})",
                   state.highestPacketSequence, packetSequence, difference);
    if (difference >= 64)
    {
      state.receivedPacketBits = 0;
    }
    else
    {
      state.receivedPacketBits <<= difference;

      // The old highest sequence is now difference positions behind.
      state.receivedPacketBits |= uint64_t{1} << (difference - 1);
    }

    state.highestPacketSequence = packetSequence;
    state.ackPending = true;
    return true;
  }

  const uint64_t difference = state.highestPacketSequence - packetSequence;

  if (difference == 0)
  {
    logger_->trace("Duplicate packet {}", packetSequence);
    return false;
  }

  if (difference > 64)
  {
    logger_->trace("Discarding stale packet {} (highest={}, delta={})",
                   packetSequence, state.highestPacketSequence, difference);
    return false;
  }

  const uint64_t bit = uint64_t{1} << (difference - 1);

  if ((state.receivedPacketBits & bit) != 0)
  {
    logger_->trace("Duplicate packet {} (bitmap)", packetSequence);
    return false;
  }
  logger_->trace("Accepted out-of-order packet {} (highest={}, delta={})",
                 packetSequence, state.highestPacketSequence, difference);
  state.receivedPacketBits |= bit;
  state.ackPending = true;
  return true;
}
void AtlasNet::Network::Cluster::ClusterChannelV1::ProcessMessage(
    const AtlasNetNodeID& source, ReceiveState& state, uint64_t messageSequence,
    std::shared_ptr<const std::vector<std::byte>> storage, size_t offset,
    size_t size)
{
  switch (GetOptions().ordering)
  {
  case OrderingMode::Unordered:
    logger_->trace("Delivered unordered message {}", messageSequence);
    readyMessages_.push_back({
        .source = source,
        .sequence = messageSequence,
        .storage = std::move(storage),
        .offset = offset,
        .size = size,
    });
    break;

  case OrderingMode::Sequenced:
    if (messageSequence <= state.highestSequencedMessage)
    {
      logger_->trace("Dropped stale sequenced message {}", messageSequence);
      return;
    }

    state.highestSequencedMessage = messageSequence;

    readyMessages_.push_back({
        .source = source,
        .sequence = messageSequence,
        .storage = std::move(storage),
        .offset = offset,
        .size = size,
    });
    break;

  case OrderingMode::Ordered:
    if (messageSequence < state.nextOrderedSequence)
    {
      // Duplicate or very late message.
      return;
    }

    if (messageSequence == state.nextOrderedSequence)
    {
      readyMessages_.push_back({
          .source = source,
          .sequence = messageSequence,
          .storage = std::move(storage),
          .offset = offset,
          .size = size,
      });

      ++state.nextOrderedSequence;
      DeliverOrderedMessages(source, state);
      return;
    }

    // A previous message is missing.
    state.reorderBuffer.try_emplace(messageSequence,
                                    BufferedMessage{
                                        .sequence = messageSequence,
                                        .storage = std::move(storage),
                                        .offset = offset,
                                        .size = size,
                                    });

    break;
  }
}
void AtlasNet::Network::Cluster::ClusterChannelV1::DeliverOrderedMessages(
    const AtlasNetNodeID& source, ReceiveState& state)
{
  while (true)
  {
    auto iterator = state.reorderBuffer.find(state.nextOrderedSequence);

    if (iterator == state.reorderBuffer.end())
      break;

    BufferedMessage buffered = std::move(iterator->second);

    state.reorderBuffer.erase(iterator);
    logger_->trace("Released buffered ordered message {}", buffered.sequence);
    readyMessages_.push_back({
        .source = source,
        .sequence = buffered.sequence,
        .storage = std::move(buffered.storage),
        .offset = buffered.offset,
        .size = buffered.size,
    });

    ++state.nextOrderedSequence;
  }
}
size_t AtlasNet::Network::Cluster::ClusterChannelV1::CopyReadyMessages(
    std::span<ClusterMessage> output)
{
  size_t count = 0;

  while (count < output.size() && !readyMessages_.empty())
  {
    ReadyMessage ready = std::move(readyMessages_.front());

    readyMessages_.pop_front();

    output[count++] =
        ClusterMessage(ready.source, GetOptions().id, ready.sequence,
                       std::move(ready.storage), ready.offset, ready.size);
  }
  if (count > 0)
    logger_->trace("Returned {} ready messages", count);
  return count;
}
std::vector<std::byte>
AtlasNet::Network::Cluster::ClusterChannelV1::BuildPacket(
    PeerState& peer, std::span<const QueuedMessage> messages,
    uint64_t packetSequence, bool ackOnly)
{
  ChannelV1::PacketHeader header;
  header.channel = GetOptions().id;
  header.packetSequence = packetSequence;
  header.ackSequence = peer.receive.highestPacketSequence;
  header.ackBits = peer.receive.receivedPacketBits;
  header.messageCount = static_cast<uint16_t>(messages.size());

  if (GetOptions().delivery == DeliveryMode::Reliable)
    header.flags = header.flags | ChannelV1::PacketFlags::Reliable;

  if (ackOnly)
    header.flags = header.flags | ChannelV1::PacketFlags::AckOnly;

  size_t payloadBytes = 0;

  for (const QueuedMessage& message : messages)
  {
    payloadBytes +=
        ChannelV1::MessageHeader::NetSize() + message.payload.size();
  }

  header.payloadBytes = static_cast<uint32_t>(payloadBytes);
  NetBinaryWriter writer;

  header.serialize(writer);

  for (const QueuedMessage& message : messages)
  {
    ChannelV1::MessageHeader messageHeader;
    messageHeader.messageSequence = message.sequence;
    messageHeader.payloadBytes = static_cast<uint32_t>(message.payload.size());
    messageHeader.serialize(writer);
    writer->adapter().writeBuffer<sizeof(uint8_t)>(
        reinterpret_cast<const uint8_t*>(message.payload.data()),
        message.payload.size());
  }

  peer.receive.ackPending = false;

  return writer.Release();
}
void AtlasNet::Network::Cluster::ClusterChannelV1::SendPacket(
    const AtlasNetNodeID& destination, PeerState& peer,
    std::vector<std::byte> bytes, uint64_t packetSequence)
{

  GetTransport()->Send(destination, bytes);

  if (GetOptions().delivery != DeliveryMode::Reliable)
    return;

  PendingPacket pending;
  pending.packetSequence = packetSequence;
  pending.bytes = std::move(bytes);
  pending.lastSent = Clock::now();
  pending.attempts = 1;

  peer.send.pendingPackets.emplace(packetSequence, std::move(pending));
}
