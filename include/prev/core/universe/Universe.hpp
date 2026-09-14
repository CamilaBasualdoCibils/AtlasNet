#pragma once
#include "WorldConcepts.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/database/redis/Redis.hpp"
#include "atlasnet/core/database/redis/RedisConn.hpp"
#include "atlasnet/core/events/GlobalEventSystem.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include "atlasnet/core/universe/WorldEnums.hpp"
#include "enviroment/Enviroment.hpp"
#include <optional>
#include <string>
namespace AtlasNet
{
class AtlasNetController;
class IAtlasNetNode;
class Universe
{
  friend AtlasNetController;
  struct WorldEntry
  {
    WorldID id;
    WorldDefinition definition;
  };

public:
  struct Config
  {
    GlobalEventSystem* _globalEventSystem;
    Database::RedisConn* __redisConfig;
  };
  Universe(const Config& config)
      : _globalEventSystem(config._globalEventSystem),
        _redisConn(config.__redisConfig) {
          assert(_globalEventSystem && "GlobalEventSystem pointer cannot be null");
          assert(_redisConn && "RedisConn pointer cannot be null");
        };

  std::optional<WorldID> GetWorld(std::string_view name)
  {
    auto worldIDStr =
        _redisConn->HashMap().GetSet().HGet(WorldNameToIDHashKey, name);
    if (!worldIDStr)
    {
      return std::nullopt;
    }
    return WorldID::from_string(*worldIDStr);
  }

protected:
  std::pair<WorldCreationResult, std::optional<WorldID>>
  CreateWorld(const WorldDefinition& def);

private:
std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("Universe");
  GlobalEventSystem* _globalEventSystem;
  Database::RedisConn* _redisConn;
  const std::string UniverseNamePrefix =
      Env::DatabaseNamespace + "{ATLASNET_UNIVERSE}Universe:";
  const std::string WorldDefinitionsHashKey =
      UniverseNamePrefix + "Worlds"; // ID -> WorldDefinition
  const std::string WorldNameToIDHashKey =
      UniverseNamePrefix + "WorldNameToID"; // Name -> ID
};
} // namespace AtlasNet