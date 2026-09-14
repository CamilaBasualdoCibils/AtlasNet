#pragma once

#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/geometry/AABB.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "boost/describe/enum.hpp"

namespace AtlasNet
{
enum class ColliderType
{
  Box,
  Sphere
};
BOOST_DESCRIBE_ENUM(ColliderType, Box, Sphere)
class ICollider
{
public:
  virtual ~ICollider() = default;

  virtual ColliderType get_type() const = 0;
  virtual void to_json(_Json& j) const = 0;
  virtual void from_json(const _Json& j) = 0;
};

class BoxCollider : public ICollider
{
  AtlasNet::AABB3f bounds;

public:
  explicit BoxCollider(const AtlasNet::AABB3f& bounds) : bounds(bounds) {}
  explicit BoxCollider(const vec3& min, const vec3& max) : bounds(min, max) {}
  explicit BoxCollider(const _Json& j)
  {
    from_json(j);
  }
  explicit BoxCollider(vec3 size) : bounds(-size * 0.5f, size * 0.5f) {}
  explicit BoxCollider(float width, float height, float depth)
      : bounds(vec3(-width, -height, -depth) * 0.5f,
               vec3(width, height, depth) * 0.5f)
  {
  }

  ColliderType get_type() const override
  {
    return ColliderType::Box;
  }
  void to_json(_Json& j) const override
  {
    j = _Json{
        {"bounds",
         {{"min",
           {{"x", bounds.min.x}, {"y", bounds.min.y}, {"z", bounds.min.z}}},
          {"max",
           {{"x", bounds.max.x}, {"y", bounds.max.y}, {"z", bounds.max.z}}}}}};
  }
  void from_json(const _Json& j) override
  {
    bounds.min.x = j.at("bounds").at("min").at("x").get<float>();
    bounds.min.y = j.at("bounds").at("min").at("y").get<float>();
    bounds.min.z = j.at("bounds").at("min").at("z").get<float>();
    bounds.max.x = j.at("bounds").at("max").at("x").get<float>();
    bounds.max.y = j.at("bounds").at("max").at("y").get<float>();
    bounds.max.z = j.at("bounds").at("max").at("z").get<float>();
  }
};

class SphereCollider : public ICollider
{
  float radius;

public:
  explicit SphereCollider(float radius) : radius(radius) {}
  ColliderType get_type() const override
  {
    return ColliderType::Sphere;
  }
  void to_json(_Json& j) const override
  {
    j = _Json{{"radius", radius}};
  }
  void from_json(const _Json& j) override
  {
    radius = j.at("radius").get<float>();
  }
};
} // namespace AtlasNet