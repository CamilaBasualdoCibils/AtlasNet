#pragma once

#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/entity/Entity.hpp"
namespace AtlasNet
{
enum class ClientConnectionResult : uint8_t
{
  Success = 0,
  Failure = 1
};
BOOST_DESCRIBE_ENUM(ClientConnectionResult, Success, Failure);
struct ClientConnectionCompleteData
{

  ClientConnectionResult result;
  AtlasNetClientID clientID;
  AtlasNetEntityID entityID;

  template <typename Archive>
 void serialize(Archive& ar)
  {
    ar(result);
    ar(clientID);
    ar(entityID);
  }
};
using ClientRPC_ClientConnectionCompleteNotification =
    RPC<"ClientConnectionCompleteNotification", void, ClientConnectionCompleteData>;

}; // namespace AtlasNet