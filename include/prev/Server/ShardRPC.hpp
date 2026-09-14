#pragma once

#include "atlasnet/core/CmdSig/command/Command.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
namespace AtlasNet
{
// Received by the backend of the shard
struct ShardSpawnClientRequest
{
  Entity::Position spawnTransform;
  AtlasNetClientID clientID;
  AtlasNetGatewayID gatewayRelayID;
  std::vector<uint8_t>
      clientSpawnPayload; // This contains developer-defined data that was given
  // by the login services that is specific to this
  // client

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(spawnTransform, clientID, gatewayRelayID, clientSpawnPayload);
  }
};

struct ShardSpawnClientResponse
{
  AtlasNetEntityID entityID;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(entityID);
  }
};
using ShardRPC_SpawnClient =
    RPC<"Shard_SpawnClient", ShardSpawnClientResponse, ShardSpawnClientRequest>;
using ShardRPC_ClientTransitCommand =
    RPC<"Shard_ClientTransitCommand", CommandAck, TransitCommandEnvelope>;

}; // namespace AtlasNet