#pragma once

#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"

#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"

namespace AtlasNet
{
using ControllerRPC_GetClosestShardToLocation =
    RPC<"Controller_GetClosestShardToLocation", AtlasNetShardID, Entity::Location>;

} // namespace AtlasNet