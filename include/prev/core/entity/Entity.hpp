#pragma once
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include "atlasnet/core/utils/Snowflake.hpp"
#include "atlasnet/core/utils/UUID.hpp"
#include "boost/container/small_vector.hpp"
#include "boost/describe/enum_from_string.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "entt/entity/fwd.hpp"
#include <entt/entt.hpp>
#include <ostream>
#include <type_traits>
#include <variant>

namespace AtlasNet
{

namespace Entity
{

#define ENTT_STANDARD_CPP

enum class CoordinateSystem
{
  Cartesian = 0,
  Geospatial = 1
};
BOOST_DESCRIBE_ENUM(CoordinateSystem, Cartesian, Geospatial);
struct CartesianPosition
{
  dvec3 position;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(position[0], position[1], position[2]);
  }
};
struct GeospatialPosition
{
  double latitude;
  double longitude;
  double altitude;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(latitude, longitude, altitude);
  }
};
struct Position
{
  std::variant<CartesianPosition, GeospatialPosition> position;

  CartesianPosition& Cartesian()
  {
    if (!std::holds_alternative<CartesianPosition>(position))
    {
      throw std::runtime_error("Position does not hold a CartesianPosition");
    }
    return std::get<CartesianPosition>(position);
  }
  GeospatialPosition& Geospatial()
  {
    if (!std::holds_alternative<GeospatialPosition>(position))
    {
      throw std::runtime_error("Position does not hold a GeospatialPosition");
    }
    return std::get<GeospatialPosition>(position);
  }
  void to_json(_Json& j) const
  {
    std::visit(
        [&j](const auto& pos)
        {
          using T = std::decay_t<decltype(pos)>;

          if constexpr (std::is_same_v<T, CartesianPosition>)
          {
            j["type"] = boost::describe::enum_to_string(
                CoordinateSystem::Cartesian, "UNKNOWN");
            j["position"] = {{"x", pos.position.x},
                             {"y", pos.position.y},
                             {"z", pos.position.z}};
          }
          else if constexpr (std::is_same_v<T, GeospatialPosition>)
          {
            j["type"] = boost::describe::enum_to_string(
                CoordinateSystem::Geospatial, "UNKNOWN");
            j["position"] = {{"latitude", pos.latitude},
                             {"longitude", pos.longitude},
                             {"altitude", pos.altitude}};
          }
        },
        position);
  }
  void from_json(const _Json& j)
  {
    if (!j.contains("type") || !j.contains("position"))
    {
      position = CartesianPosition{{0.0, 0.0, 0.0}};
      return;
    }

    const auto typeStr = j.at("type").get<std::string>();
    CoordinateSystem spaceType{};
    if (!boost::describe::enum_from_string(typeStr.c_str(), spaceType))
    {
      position = CartesianPosition{{0.0, 0.0, 0.0}};
      return;
    }

    if (spaceType == CoordinateSystem::Cartesian)
    {
      const auto& posJson = j.at("position");
      position = CartesianPosition{{posJson.at("x").get<double>(),
                                    posJson.at("y").get<double>(),
                                    posJson.at("z").get<double>()}};
    }
    else if (spaceType == CoordinateSystem::Geospatial)
    {
      const auto& posJson = j.at("position");
      position = GeospatialPosition{posJson.at("latitude").get<double>(),
                                    posJson.at("longitude").get<double>(),
                                    posJson.at("altitude").get<double>()};
    }
  }
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(position);
  }

  std::string to_string() const
  {
    std::ostringstream oss;
    std::visit(
        [&oss](const auto& pos)
        {
          using T = std::decay_t<decltype(pos)>;

          if constexpr (std::is_same_v<T, CartesianPosition>)
          {
            oss << "Cartesian Position: (" << pos.position.x << ", "
                << pos.position.y << ", " << pos.position.z << ")";
          }
          else if constexpr (std::is_same_v<T, GeospatialPosition>)
          {
            oss << "Geospatial Position: (Latitude: " << pos.latitude
                << ", Longitude: " << pos.longitude
                << ", Altitude: " << pos.altitude << ")";
          }
        },
        position);
    return oss.str();
  }
  friend std::ostream& operator<<(std::ostream& os, const Position& pos)
  {
    os << pos.to_string();
    return os;
  }
};
struct Location
{
  WorldID worldId;
  Position position;

  void to_json(_Json& j) const
  {
    _Json positionJson;
    position.to_json(positionJson);
    j = _Json{{"worldId", worldId.to_string()}, {"position", positionJson}};
  }

  void from_json(const _Json& j)
  {
    worldId = (WorldID)WorldID::from_string(j.at("worldId").get<std::string>());
    position.from_json(j.at("position"));
  }
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(worldId, position);
  }

  friend std::ostream& operator<<(std::ostream& os, const Location& loc)
  {
    os << "WorldID: " << loc.worldId.to_string() << ", " << loc.position;
    return os;
  }
};
namespace Components
{
struct EntityComponent
{
};
struct BaseEntityInfo
{
  Location location;
  void to_json(_Json& j) const
  {

    _Json locationJson;
    location.to_json(locationJson);
    j = _Json{{"location", locationJson}};
  }
  void from_json(const _Json& j)
  {
    location.from_json(j.at("location"));
  }
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(location);
  }
};
struct EntityInfo : public EntityComponent
{
  AtlasNetEntityID id;
  BaseEntityInfo baseInfo;
  void to_json(_Json& j) const
  {
    _Json baseInfoJson;
    baseInfo.to_json(baseInfoJson);
    j = _Json{{"id", id.to_string()}, {"baseInfo", baseInfoJson}};
  }
  void from_json(const _Json& j)
  {
    std::optional<AtlasNetEntityID> optId =
        AtlasNetEntityID::from_string(j.at("id").get<std::string>());
    if (optId.has_value())
    {
      id = optId.value();
    }
    else
    {
      throw std::runtime_error("Invalid EntityID string");
    }
    baseInfo.from_json(j.at("baseInfo"));
  };
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(id, baseInfo);
  }
};
struct ActorInfo : public EntityComponent
{
  int pad;
  // Data is not required because we dont store, we serialize on transform
  // from IAtlasNetShard boost::container::small_vector<uint8_t, 256> payload;
  void to_json(_Json& j) const
  {
    j = _Json{};
  }
  void from_json(const _Json& j) {}
};
struct ClientInfo : public EntityComponent
{
  AtlasNetClientID id;
  void to_json(_Json& j) const
  {
    j = _Json{{"id", id.to_string()}};
  }
  void from_json(const _Json& j)
  {
    std::optional<AtlasNetClientID> optId =
        AtlasNetClientID::from_string(j.at("id").get<std::string>());
    if (optId.has_value())
    {
      id = optId.value();
    }
    else
    {
      throw std::runtime_error("Invalid ClientID string");
    }
  }
};
struct ColliderInfo : public EntityComponent
{
  std::variant<std::monostate, BoxCollider, SphereCollider> collider;

  void to_json(_Json& j) const
  {
    j = _Json::object();

    std::visit(
        [&j](const auto& col)
        {
          using T = std::decay_t<decltype(col)>;

          if constexpr (std::is_same_v<T, std::monostate>)
          {
            j["type"] = "NONE";
          }
          else
          {
            j["type"] =
                boost::describe::enum_to_string(col.get_type(), "UNKNOWN");
            col.to_json(j);
          }
        },
        collider);
  }

  void from_json(const _Json& j)
  {
    if (!j.contains("type"))
    {
      collider = std::monostate{};
      return;
    }

    const auto typeStr = j.at("type").get<std::string>();
    if (typeStr == "NONE")
    {
      collider = std::monostate{};
      return;
    }

    ColliderType type{};
    if (!boost::describe::enum_from_string(typeStr.c_str(), type))
    {
      collider = std::monostate{};
      return;
    }

    if (type == ColliderType::Box)
    {
      collider = BoxCollider(j);
    }
    else if (type == ColliderType::Sphere)
    {
      collider = SphereCollider(j);
    }
    else
    {
      collider = std::monostate{};
    }
  }
};

} // namespace Components

// using EntityTabl2 = entt::basic_registry<Entitye>;
using EntityTable = entt::registry;
struct Entity_Serializable
{
  std::optional<Components::EntityInfo> entityInfo;
  std::optional<Components::ActorInfo> actorInfo;
  std::optional<Components::ClientInfo> clientInfo;
  std::optional<Components::ColliderInfo> colliderInfo;

  void to_json(_Json& j) const
  {
    if (entityInfo)
      entityInfo->to_json(j["entityInfo"]);
    if (actorInfo)
      actorInfo->to_json(j["actorInfo"]);
    if (clientInfo)
      clientInfo->to_json(j["clientInfo"]);
    if (colliderInfo)
      colliderInfo->to_json(j["colliderInfo"]);
  }
  void from_json(const _Json& j)
  {
    if (j.contains("entityInfo"))
    {
      entityInfo.emplace();
      entityInfo->from_json(j.at("entityInfo"));
    }
    if (j.contains("actorInfo"))
    {
      actorInfo.emplace();
      actorInfo->from_json(j.at("actorInfo"));
    }
    if (j.contains("clientInfo"))
    {
      clientInfo.emplace();
      clientInfo->from_json(j.at("clientInfo"));
    }
    if (j.contains("colliderInfo"))
    {
      colliderInfo.emplace();
      colliderInfo->from_json(j.at("colliderInfo"));
    }
  };
};
} // namespace Entity
} // namespace AtlasNet
