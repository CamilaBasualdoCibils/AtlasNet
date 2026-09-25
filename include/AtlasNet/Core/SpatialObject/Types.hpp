#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <cstdint>
#include <optional>

namespace AtlasNet::SpatialObject
{

struct SpatialObjectHandle
{
  uint32_t slot = 0;
  uint32_t generation = 0;

  constexpr bool operator==(const SpatialObjectHandle&) const = default;
  [[nodiscard]] constexpr bool IsValid() const
  {
    return generation != 0;
  }
};

enum class CoordinateSystem : uint8_t
{
  Cartesian = 0,
  Geospatial = 1
};

struct RoleFlags
{
  uint8_t isActor : 1 = 0;
  uint8_t isClient : 1 = 0;
  uint8_t alive : 1 = 0;
  uint8_t reserved : 5 = 0;

  constexpr bool operator==(const RoleFlags&) const = default;
};

/** Hot fields published for lock-free readers (seqlock-protected). */
struct PublishedState
{
  AtlasNetEntityID id{};
  AtlasNetNodeID worldId{};
  CoordinateSystem coordinateSystem = CoordinateSystem::Cartesian;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  RoleFlags roles{};
  AtlasNetClientID clientId{};
  uint64_t commandSequence = 0;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    uint8_t roleBits = static_cast<uint8_t>(
        (roles.isActor ? 1 : 0) | (roles.isClient ? 2 : 0) |
        (roles.alive ? 4 : 0));
    ar(id, worldId, coordinateSystem, x, y, z, clientId, commandSequence,
       roleBits);
    roles.isActor = (roleBits & 1) != 0;
    roles.isClient = (roleBits & 2) != 0;
    roles.alive = (roleBits & 4) != 0;
  }
};

enum class InsertResult : uint8_t
{
  Ok,
  AlreadyExists,
  TableFull
};

enum class RemoveResult : uint8_t
{
  Ok,
  NotFound,
  StaleHandle
};

} // namespace AtlasNet::SpatialObject
