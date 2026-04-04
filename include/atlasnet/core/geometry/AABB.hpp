#pragma once

#include "Vec.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <string>
#include <format>
namespace AtlasNet {

template <uint8_t Dim, typename Type, glm::qualifier Q = glm::defaultp>
struct AABB_old {
  static_assert(Dim > 0, "AABB dimension must be > 0");

  using value_type = Type;
  using vectype = glm::vec<Dim, Type, Q>;

  vectype min = vectype(0);
  vectype max = vectype(0);

  // -------------------------------------------------
  // Constructors
  // -------------------------------------------------

  // Invalid / empty box
  AABB_old() : min(vectype(0)), max(vectype(0)) {}

  AABB_old(const vectype &min_, const vectype &max_)
      : min(glm::min(min_, max_)), max(glm::max(min_, max_)) {}

  std::string ToString() const {
    return std::format("Min: {}, Max: {}", glm::to_string(min),
                       glm::to_string(max));
  }
  // From center + half extents
  static AABB_old FromCenterExtents(const vectype &center,
                                const vectype &halfExtents) {
    return AABB_old(center - halfExtents, center + halfExtents);
  }
  void SetCenterExtents(const vectype &center, const vectype &halfExtents) {
    *this = FromCenterExtents(center, halfExtents);
  }
  void Serialize(ByteWriter &bw) const {
    bw.write_vector<Dim>(min);
    bw.write_vector<Dim>(max);
  }
  void Deserialize(ByteReader &br) {
    min = br.read_vector<Dim, decltype(min)>();
    max = br.read_vector<Dim, decltype(max)>();
  }

  template <typename archive> void serialize(archive &ar) {
    ar(min);
    ar(max);
  }

  // -------------------------------------------------
  // Properties
  // -------------------------------------------------

  vectype center() const { return (min + max) * Type(0.5); }

  vectype size() const { return max - min; }

  vectype halfExtents() const { return (max - min) * Type(0.5); }

  Type volume() const {
    vectype s = size();
    Type v = Type(1);
    for (uint8_t i = 0; i < Dim; ++i)
      v *= s[i];
    return v;
  }

  bool valid() const {
    for (uint8_t i = 0; i < Dim; ++i) {
      if (min[i] > max[i])
        return false;
    }
    return true;
  }

  // -------------------------------------------------
  // Expansion
  // -------------------------------------------------

  void expand(const vectype &p) {
    min = glm::min(min, p);
    max = glm::max(max, p);
  }

  void expand(const AABB_old &other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
  }

  void pad(Type amount) {
    vectype v(amount);
    min -= v;
    max += v;
  }

  float square_distance(vectype p) const
  {
    float sqDist = 0.0f;
    for (uint8_t i = 0; i < Dim; ++i)
    {
      float v = 0.0f;
      if (p[i] < min[i])
        v = min[i] - p[i];
      else if (p[i] > max[i])
        v = p[i] - max[i];
      sqDist += v * v;
    }
    return sqDist;
  }
  float distance(vectype p) const
  {
    return std::sqrt(square_distance(p));
  }
  // -------------------------------------------------
  // Containment
  // -------------------------------------------------

  bool contains(const vectype &p) const {
    for (uint8_t i = 0; i < Dim; ++i) {
      if (p[i] < min[i] || p[i] > max[i])
        return false;
    }
    return true;
  }

  bool contains(const AABB_old &other) const {
    return contains(other.min) && contains(other.max);
  }

  // -------------------------------------------------
  // Intersection
  // -------------------------------------------------

  bool intersects(const AABB_old &other) const {
    for (uint8_t i = 0; i < Dim; ++i) {
      if (other.min[i] > max[i] || other.max[i] < min[i])
        return false;
    }
    return true;
  }

  AABB_old intersection(const AABB_old &other) const {
    AABB_old result(glm::max(min, other.min), glm::min(max, other.max));

    if (!result.valid())
      return AABB_old();

    return result;
  }

  // -------------------------------------------------
  // Ray intersection (slab method, Dim-agnostic)
  // -------------------------------------------------

  bool intersectsRay(const vectype &rayOrigin, const vectype &rayDir,
                     Type &tMin, Type &tMax) const {
    tMin = Type(0);
    tMax = std::numeric_limits<Type>::max();

    const Type eps = Type(1e-8);

    for (uint8_t i = 0; i < Dim; ++i) {
      if (std::abs(rayDir[i]) < eps) {
        // Ray parallel to slab
        if (rayOrigin[i] < min[i] || rayOrigin[i] > max[i])
          return false;
      } else {
        Type invD = Type(1) / rayDir[i];
        Type t0 = (min[i] - rayOrigin[i]) * invD;
        Type t1 = (max[i] - rayOrigin[i]) * invD;

        if (t0 > t1)
          std::swap(t0, t1);

        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;

        if (tMin > tMax)
          return false;
      }
    }

    return true;
  }
};
using AABB2i = AABB_old<2, int32_t>;
using AABB3i = AABB_old<3, int32_t>;
using AABB2f = AABB_old<2, float>;
using AABB3f = AABB_old<3, float>;

} // namespace AtlasNet