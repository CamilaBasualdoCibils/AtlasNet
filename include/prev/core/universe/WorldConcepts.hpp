#pragma once

// Usage:
/*
ATLASNET_WORLD_DEFINITION(Name, Space System, Heuristic)*/
#include "WorldEnums.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/utils/UUID.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "boost/describe/enum_from_string.hpp"
#include "boost/describe/enum_to_string.hpp"
#include <cstdint>
#include <string>
namespace AtlasNet
{
using WorldID = UUID;
struct WorldDefinition
{
  std::string name;
  WorldSpaceType spaceType;

  void Serialize(ByteWriter& writer) const
  {
    writer.str(name);
    writer.i32(static_cast<int32_t>(spaceType));
  }

  void Deserialize(ByteReader& reader)
  {
    reader.str(name);
    int32_t spaceTypeInt;
    reader.i32(spaceTypeInt);
    spaceType = static_cast<WorldSpaceType>(spaceTypeInt);
  }
  void to_json(_Json& j) const
  {
    j = _Json{
        {"name", name},
        {"spaceType", boost::describe::enum_to_string(spaceType, "UNKNOWN")}};
  }
  void from_json(const _Json& j)
  {
    name = j.at("name").get<std::string>();
    const auto spaceTypeStr = j.at("spaceType").get<std::string>();
    WorldSpaceType st{};
    if (!boost::describe::enum_from_string(spaceTypeStr.c_str(), st))
    {
      throw std::runtime_error("Invalid space type in JSON: " + spaceTypeStr);
    }
    else
    {
      spaceType = st;
    }
  }
};
} // namespace AtlasNet
