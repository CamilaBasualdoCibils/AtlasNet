#include "atlasnet/core/entity/EntityLedger.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/entity/EntityLedgerRPC.hpp"
#include <unordered_map>

void AtlasNet::Entity::EntityLedger::SetRPCBinds()
{
  _config.rpcSystem->Bind<EntityLedger_GetAllEntitiesInfoRPC>(
      [this]()
      {
        logger->info("RPC call received: GetAllEntitiesInfo");
        std::unordered_map<AtlasNetEntityID, Components::EntityInfo> allInfo;
        {
          auto access = GetReadAccess();

          logger->info("Entity Count: {}", IDMapping.size());
          for (const auto& [id, enttId] : IDMapping.left)
          {
            const auto& entityInfo = access.GetEntityInfo(id);
            allInfo[id] = entityInfo;
            logger->info("Entity ID: {}\npos: {}", id.to_string(), entityInfo.baseInfo.location.position.to_string());
          }
        }
        return allInfo;
      });
}