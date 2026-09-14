#pragma once

#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/database/redis/Redis.hpp"
#include "atlasnet/core/entity/Entity.hpp"

#include "enviroment/Enviroment.hpp"
namespace AtlasNet
{
class EntityRegistry
{
public:
  struct Config
  {
    Database::RedisConn* _redisConn;
  };
  EntityRegistry(const Config& config) : config_(config) {}
  void SetEntityToShard(const AtlasNetEntityID& id,
                        const AtlasNetShardID& shardId)
  {
    config_._redisConn->HashMap().GetSet().HSet(
        EntityID2ShardIDMapKey, id.to_string(), shardId.to_string());
  }

  std::optional<AtlasNetShardID> GetShardForEntity(const AtlasNetEntityID& id)
  {
    auto result = config_._redisConn->HashMap().GetSet().HGet(
        EntityID2ShardIDMapKey, id.to_string());
    if (result)
    {
      return AtlasNetShardID::from_string(*result);
    }
    return std::nullopt;
  }
  void RemoveEntity(const AtlasNetEntityID& id)
  {
    config_._redisConn->HashMap().Delete().HDel(EntityID2ShardIDMapKey,
                                                id.to_string());
  }

private:
  const Config config_;
  const std::string EntityRegistryPrefix = Env::DatabaseNamespace + "Entity:";
  const std::string EntityID2ShardIDMapKey =
      EntityRegistryPrefix + "EntityID2ShardIDMap";
};
}; // namespace AtlasNet