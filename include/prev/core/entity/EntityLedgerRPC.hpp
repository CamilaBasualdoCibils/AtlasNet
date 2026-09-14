#pragma once

#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include <utility>
#include <vector>
namespace AtlasNet
{
using EntityLedger_GetEntityInfoRPC =
    RPC<"EntityLedger_GetEntityInfo", AtlasNet::Entity::Components::EntityInfo,
        AtlasNet::AtlasNetEntityID>;
using EntityLedger_GetAllEntitiesInfoRPC =
    RPC<"EntityLedger_GetAllEntitiesInfo",
        std::unordered_map<AtlasNet::AtlasNetEntityID,
                           AtlasNet::Entity::Components::EntityInfo>>;
}

/* ATLASNET_RPC(
    EntityLedgerRPC,
    ATLASNET_RPC_METHOD(
        GetEntityInfo,
        ATLASNET_RPC_SIG(AtlasNet::Entity::Components::EntityInfo(
            AtlasNet::AtlasNetEntityID)));
    ATLASNET_RPC_METHOD(
        GetAllEntitiesInfo,
        ATLASNET_RPC_SIG(
            std::unordered_map<AtlasNet::AtlasNetEntityID,
                               AtlasNet::Entity::Components::EntityInfo>())));
 */
