#pragma once
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Messages/Message.hpp"
#include "AtlasNet/Core/Network/Messages/MessageRecepient.hpp"
#include "AtlasNet/Core/Network/NetworkPacket.hpp"
#include "AtlasNet/Core/Network/Transport/Datagram/IDatagramTransport.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
namespace AtlasNet::Network::Messages
{
class InternalMessenger
{
public:
  struct Config
  {
    SocketAddress DBAddress;
    std::shared_ptr<Network::IDatagramTransport> transport;
  };
  InternalMessenger(const Config& config) : config(config) {}
  void Send(MessageID id, Network::PacketPayloadView data,
            const VRecepient& recepient)
  {
    NetBinaryWriter writer;
    writer(id);
    writer(data);
    std::visit([&](auto&& recepient)
               { ResolveAndSend(writer.GetBytes(), recepient); }, recepient);
  }
  template <typename Message>
  void Send(const Message::PayloadType& payload, const VRecepient& recepient)
  {

    NetBinaryWriter writer;
    writer(Message::ID);
    writer(payload);
    std::visit([&](auto&& recepient)
               { ResolveAndSend(writer.GetBytes(), recepient); }, recepient);
  }

private:
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("InternalMessenger");
  Config config;
  void ResolveAndSend(Network::PacketPayloadView data,
                      const Recepient::NodeRecepient& recepient)
  {
    // Implementation for sending to a node
  }
  void ResolveAndSend(Network::PacketPayloadView data,
                      const Recepient::ShardOfEntityRecepient& recepient)
  {
    // Implementation for sending to a shard of an entity
  }
  void ResolveAndSend(Network::PacketPayloadView data,
                      const Recepient::ShardOfClientRecepient& recepient)
  {
    // Implementation for sending to a shard of a client
  }
  void ResolveAndSend(Network::PacketPayloadView data,
                      const Recepient::DatabaseRecepient& recepient)
  {
    logger->info("MSG({} bytes) -> DB at {}", data.size(),
                 config.DBAddress.to_string());
    __Send(data, config.DBAddress);
  }

  void __Send(Network::PacketPayloadView data, const SocketAddress& address)
  {
    config.transport->SendMessage(address, data);
  }
};
} // namespace AtlasNet::Network::Messages