#pragma once
#include "atlasnet/core/UUID.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "boost/container/small_vector.hpp"
#include "entt/entity/fwd.hpp"
#include <entt/entt.hpp>
#include <variant>
namespace AtlasNet
{

struct EntityIDTag
{
};
struct ClientIDTag
{
};
using EntityID = StrongUUID<EntityIDTag>;

#define ENTT_STANDARD_CPP
using ClientID = StrongUUID<ClientIDTag>;
using WorldID = uint32_t;
struct Transform
{
  vec3 position;
};
struct Location
{
  WorldID worldId;
  Transform transform;
};
struct EntityInfo
{
  EntityID id;
  Location location;
};
struct ActorInfo
{
  //Data is not required because we dont store, we serialize on transform from IAtlasNetShard
  //boost::container::small_vector<uint8_t, 256> payload;
};
struct ClientInfo
{
  ClientID id;
};
struct Collider
{
  std::variant< BoxCollider, SphereCollider> collider;
};

using EntityTable = entt::registry;
} // namespace AtlasNet