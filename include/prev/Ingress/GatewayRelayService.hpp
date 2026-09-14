#pragma once

#include "atlasnet/core/CmdSig/command/CommandEnums.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/RPC/RPCSystem.hpp"
#include "atlasnet/core/network/address/AddressResolver.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/cache/Cache.hpp"
#include "atlasnet/core/client/ClientRegistry.hpp"

#include "atlasnet/core/CmdSig/command/Command.hpp"
#include "atlasnet/core/database/redis/Redis.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"

#include "atlasnet/gateway/GatewayRPC.hpp"
#include "atlasnet/shard/ShardRPC.hpp"
#include "enviroment/Enviroment.hpp"
#include "spdlog/sinks/stdout_color_sinks-inl.h"
#include <cassert>
#include <shared_mutex>

namespace AtlasNet
{
class AtlasNetGateway;
class GatewayRelayService
{

public:
  struct Config
  {
    Database::RedisConn* redisConn;
    AtlasNetGateway* gateway;
    MessageSystem* messageSystem;
    RPCSystem* rpcSystem;
    ClientRegistry* clientRegistry;
    AddressResolver* addressResolver;
    // Add any necessary configuration parameters here
  };
  GatewayRelayService(const Config& config)
      : config_(config),
        clientToAddressCache_([this](const Network::SocketAddress& address)
                              { return __GetClientIDByAddress(address); },
                              [this](const AtlasNetClientID& id)
                              { return __GetClientAddressByID(id); }),
        clientToShardCache_([this](const AtlasNetClientID& id)
                            { return __GetShardByClientID(id); })
  {
    assert(config_.redisConn && "RedisConn pointer cannot be null");
    assert(config_.gateway && "Gateway pointer cannot be null");
    assert(config_.messageSystem && "MessageSystem pointer cannot be null");
    assert(config_.clientRegistry && "ClientRegistry pointer cannot be null");
    assert(config_.addressResolver && "AddressResolver pointer cannot be null");
    assert(config_.rpcSystem && "RPCSystem pointer cannot be null");

    config_.rpcSystem->Bind<GatewayRPC_IngressCommand>(
        [this](const RPCContext& context,
               const IngressCommandEnvelope& commandEnvelope) -> CommandAck
        { return HandleIngressCommand(commandEnvelope, context); });

    /* config_.messageSystem->On<ExternalCommandMessage>(
        [&](const ExternalCommandMessage& message,
            const Network::SocketAddress& sourceAddress)
        {
          std::optional<AtlasNetClientID> cachedClientID =
              clientToAddressCache_.GetBySecond(sourceAddress);
          if (!cachedClientID)
          {
            logger->error("Received message from unknown address: {}, cannot "
                          "find associated ClientID",
                          sourceAddress.to_string());
            assert(false && "Received message from unknown address, cannot "
                            "find associated ClientID");
            return;
          }

          HandleExternalCommand(message, *cachedClientID);
        }); */
  }
  /* std::optional<AtlasNetGatewayID>
  GetManagingGateway(const AtlasNetClientID& clientID)
  {
  }
  std::optional<AtlasNetGatewayID>
  GetManagingGateway(const Network::SocketAddress& address)
  {
  } */

  /*Sets up that a given client's commands are forwarded to a set shard*/

  /*Sets up that this gateway manages a set client*/
  void DeclareGatewayRelay(const AtlasNetClientID& clientID);

private:
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("GatewayRelay");
  const Config config_;

  const std::string GatewayRelayKeyPrefix =
      Env::DatabaseNamespace + "gateway_relay{ATLASNET_GATEWAY_RELAY}:";
  const std::string ClientID2GatewayID =
      GatewayRelayKeyPrefix + "ClientID->GatewayRelay";
  const std::string GatewayID2ClientIDs_Set =
      GatewayRelayKeyPrefix + "GatewayID->ClientIDs";

  CacheMap<AtlasNetClientID, AtlasNetShardID> clientToShardCache_;

  CacheBiMap<AtlasNetClientID, Network::SocketAddress> clientToAddressCache_;

  std::string
  GatewayID2ClientIDs_SetKey(const AtlasNetGatewayID& gatewayID) const
  {
    return GatewayID2ClientIDs_Set + ":" + gatewayID.to_string();
  }
  CommandAck HandleIngressCommand(const IngressCommandEnvelope& commandEnvelope,
                                  const RPCContext& context)
  {
    logger->info("Received ingress command: {} from address: {}",
                 commandEnvelope.package.commandPayload.commandName,
                 context.sourceAddress.to_string());

    const std::optional<AtlasNetClientID> clientID =
        config_.clientRegistry->GetAddressClientID(context.sourceAddress);
    if (!clientID)
    {
      assert(clientID && "Received ingress command from unknown address, "
                         "cannot find associated ClientID");
      logger->error("Received ingress command from unknown address: {}, cannot "
                    "find associated ClientID",
                    context.sourceAddress.to_string());
      return CommandAck{CommandAckStatus::UnknownError};
    }
    const std::optional<AtlasNetEntityID> entityID =
        config_.clientRegistry->GetClientEntityID(*clientID);
    if (!entityID)
    {
      assert(entityID && "Received ingress command from unknown client, "
                         "cannot find associated EntityID");
      logger->error("Received ingress command from unknown client: {}, cannot "
                    "find associated EntityID",
                    clientID->to_string());
      return CommandAck{CommandAckStatus::UnknownError};
    }
    const std::optional<AtlasNetShardID> shardID =
        config_.addressResolver->ResolveEntityShard(*entityID);
    if (!shardID)
    {
      assert(shardID && "Received ingress command from unknown client, "
                        "cannot find associated ShardID");
      logger->error("Received ingress command from unknown client: {}, cannot "
                    "find associated ShardID",
                    clientID->to_string());
      return CommandAck{CommandAckStatus::UnknownError};
    }
    const std::optional<Network::SocketAddress> shardAddress =
        config_.addressResolver->ResolveShard(*shardID);
    if (!shardAddress)
    {
      assert(shardAddress && "Received ingress command from unknown shard, "
                             "cannot find associated ShardAddress");
      logger->error("Received ingress command from unknown shard: {}, cannot "
                    "find associated ShardAddress",
                    shardID->to_string());
      return CommandAck{CommandAckStatus::UnknownError};
    }
    TransitCommandEnvelope commandEnvelopeWithClientID{
        .targetEntity = entityID.value(),
        .commandPackage = commandEnvelope.package};

    if (commandEnvelope.package.deliveryMode ==
            CommandDeliveryGuarantee::NoDelay ||
        commandEnvelope.package.deliveryMode ==
            CommandDeliveryGuarantee::Unreliable ||
        commandEnvelope.package.deliveryMode ==
            CommandDeliveryGuarantee::UnreliableBatched)
    {
      // no Ack expected
      config_.rpcSystem->Call<ShardRPC_ClientTransitCommand>(
          *shardAddress, {commandEnvelopeWithClientID});
      return CommandAck{CommandAckStatus::GatewayAck};
    }

    auto result = config_.rpcSystem->Call_R<ShardRPC_ClientTransitCommand>(
        *shardAddress, {commandEnvelopeWithClientID});

    std::future_status status = result.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready)
    {
      auto ackResult = result.get();
      if (ackResult.has_value())
      {
        return ackResult.value();
      }
    }
    return CommandAck{CommandAckStatus::TimedOut};
  }
  /*
   void HandleExternalCommand(const ExternalCommandMessage& message,
                              const AtlasNetClientID& sourceClientID)
   {
     logger->info("Received external command: {} from ClientID: {}",
                  message.envelope.commandName, sourceClientID.to_string());

     // Deserialize the command name and payload
     const std::string_view commandName = message.envelope.commandName;

     InternalCommandEnvelope internalEnvelope;
     internalEnvelope.commandName = commandName;
     internalEnvelope.senderType = InternalCommandEnvelope::SenderType::Client;
     internalEnvelope.sender = sourceClientID;
     internalEnvelope.payload = std::move(message.envelope.payload);

     const std::optional<AtlasNetShardID> shardID =
   __GetShardByClientID(sourceClientID);


     config_.m
     // Here you can implement logic to forward the command to the appropriate
     // shard or handle it as needed.
   } */

  std::optional<Network::SocketAddress>
  __GetClientAddressByID(const AtlasNetClientID& clientID)
  {
    return config_.clientRegistry->GetClientIDAddress(clientID);
  }
  std::optional<AtlasNetClientID>
  __GetClientIDByAddress(const Network::SocketAddress& address)
  {
    return config_.clientRegistry->GetAddressClientID(address);
  }

  std::optional<AtlasNetShardID>
  __GetShardByClientID(const AtlasNetClientID& clientID)
  {
    return std::nullopt;
  }
};
} // namespace AtlasNet