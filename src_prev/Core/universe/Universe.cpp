#include "atlasnet/core/universe/Universe.hpp"
#include "atlasnet/core/universe/UniverseEvents.hpp"

std::pair<AtlasNet::WorldCreationResult, std::optional<AtlasNet::WorldID>>
AtlasNet::Universe::CreateWorld(const AtlasNet::WorldDefinition& def)
{
  std::string worldNameKey = WorldNameToIDHashKey + ":" + def.name;
  WorldID potentialWorldID =
      UUID::Generate(); // Generate a potential ID for the new world
  std::string worldIDKey;
  std::string worldDefSerialized;
  {
    ByteWriter worldDefWriter;
    def.Serialize(worldDefWriter);
    worldDefSerialized = std::string(worldDefWriter.as_string_view());
  }
  {

    worldIDKey =
        WorldDefinitionsHashKey + ":" + potentialWorldID.to_string() + ":def";
  }
  /*atomic lua script that
  1. checks if Name exists in WorldNameToID
  2. checks if ID exists in WorldDefinitions
  3. sets Name -> ID in WorldNameToID
  4. sets ID -> Definition in WorldDefinitions
  */
  std::string luaScript = R"(
      local worldNameKey = KEYS[1]
      local worldIDKey = KEYS[2]
      local worldDefSerialized = ARGV[1]

      if redis.call('EXISTS', worldNameKey) == 1 then
        return "World name already exists"
      end

      if redis.call('EXISTS', worldIDKey) == 1 then
        return "World ID collision, try again"
      end

      redis.call('SET', worldNameKey, ARGV[2])
      redis.call('SET', worldIDKey, worldDefSerialized)
      return "OK"
    )";
  std::vector<std::string> Input = {"EVAL",
                                    luaScript,
                                    "2",
                                    worldNameKey,
                                    worldIDKey,
                                    worldDefSerialized,
                                    potentialWorldID.to_string()};
  std::optional<std::string> resultStr =
      _redisConn->Command<std::string>(Input.begin(), Input.end());
  if (!resultStr)
  {
    return {WorldCreationResult::DefinitionConflict, std::nullopt};
  }
  if (Env::DebugMode)
  {
    _Json debugJson;
    def.to_json(debugJson);
    _redisConn->KeyVal().GetSet().Set(worldIDKey + "_debug", debugJson.dump());
  }
  logger->info("Created World {} with ID {}", def.name,
               potentialWorldID.to_string());

  WorldCreatedEvent event;
  event.worldID = potentialWorldID;
  event.worldName = def.name;
  event.worldDefinition = def;
  _globalEventSystem->Emit(event)->wait();
  return {WorldCreationResult::Success, potentialWorldID};
}