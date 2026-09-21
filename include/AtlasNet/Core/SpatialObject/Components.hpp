#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/SpatialObject/Types.hpp"
#include <cstdint>

namespace AtlasNet::SpatialObject
{

struct ObjectInfo
{
  AtlasNetNodeID worldId{};
  uint32_t typeTag = 0;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(worldId, typeTag);
  }
};

struct Transform
{
  CoordinateSystem coordinateSystem = CoordinateSystem::Cartesian;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(coordinateSystem, x, y, z);
  }
};

/** Marker: SpatialObject is a commandable actor. */
struct ActorTag
{
  uint8_t pad = 0;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(pad);
  }
};

struct ClientInfo
{
  AtlasNetClientID clientId{};

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(clientId);
  }
};

} // namespace AtlasNet::SpatialObject
