#pragma once
#include "atlasnet/core/geometry/AABB.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace AtlasNet
{
using Mesh = int; // Placeholder for actual mesh type
using RegionID = std::uint64_t;

template <std::size_t Dim, typename T>
concept SupportedPartitionDimension =
    (Dim == 2 || Dim == 3) && std::is_floating_point_v<T>;

template <std::size_t Dim, typename T = float>
  requires SupportedPartitionDimension<Dim, T>
class IRegionT
{
public:
  using VecType = vec<Dim, T>;

  virtual ~IRegionT() = default;

  [[nodiscard]] virtual RegionID GetID() const = 0;
  [[nodiscard]] virtual const VecType& GetCentroid() const = 0;
  [[nodiscard]] virtual const AABB_old<Dim, T>& GetAABB() const = 0;

  // Replace Mesh with your actual mesh type/include.
  [[nodiscard]] virtual const Mesh& GetMesh() const = 0;

  [[nodiscard]] virtual bool IsInside(const VecType& point) const = 0;
};

template <std::size_t Dim, typename T = float>
  requires SupportedPartitionDimension<Dim, T>
class IPartitionT
{
public:
  using VecType = vec<Dim, T>;
  using RegionType = IRegionT<Dim, T>;

  virtual ~IPartitionT() = default;

  [[nodiscard]] virtual std::size_t RegionCount() const = 0;

  [[nodiscard]] virtual const RegionType* FindRegion(RegionID id) const = 0;

  // Stable, allocation-free region view.
  [[nodiscard]] virtual std::span<const std::shared_ptr<const RegionType>>
  Regions() const = 0;

  [[nodiscard]] virtual std::optional<RegionID>
  QueryRegion(const VecType& point) const = 0;

  [[nodiscard]] virtual RegionID
  QueryClosestRegion(const VecType& point) const = 0;
};

struct RepartitionOptions
{
  float stabilityWeight = 1.0f;
  bool allowNewRegions = true;
  bool allowRegionRemoval = true;
};

class IRegionIdGenerator
{
public:
  virtual ~IRegionIdGenerator() = default;
  virtual RegionID Next() = 0;
  virtual void Release(RegionID id) = 0;
};

class SimpleRegionIdGenerator : public IRegionIdGenerator
{
  RegionID nextId = 1;

public:
  RegionID Next() override
  {
    return nextId++;
  }

  void Release(RegionID) override
  {
    // No-op
  }
};

class SafeRegionIDGenerator : public IRegionIdGenerator
{
  RegionID nextId = 1;
  std::unordered_set<RegionID> releasedIds;

public:
  RegionID Next() override
  {
    if (!releasedIds.empty())
    {
      auto it = releasedIds.begin();
      RegionID id = *it;
      releasedIds.erase(it);
      return id;
    }
    return nextId++;
  }

  void Release(RegionID id) override
  {
    releasedIds.insert(id);
  }
};

template <std::size_t Dim, typename T = float>
  requires SupportedPartitionDimension<Dim, T>
class IHeuristicT
{
public:
  using VecType = vec<Dim, T>;
  using RegionType = IRegionT<Dim, T>;
  using PartitionType = IPartitionT<Dim, T>;

  virtual ~IHeuristicT() = default;

  [[nodiscard]] virtual std::unique_ptr<PartitionType>
  Partition(std::span<const VecType> points, std::size_t desiredRegionCount,
            IRegionIdGenerator& ids) const = 0;

  [[nodiscard]] virtual std::unique_ptr<PartitionType>
  Repartition(std::span<const VecType> points, std::size_t desiredRegionCount,
              const PartitionType& previous, IRegionIdGenerator& ids,
              const RepartitionOptions& options = {}) const = 0;
};

using IRegion = IRegionT<3, float>;
using IPartition = IPartitionT<3, float>;
using IHeuristic = IHeuristicT<3, float>;

} // namespace AtlasNet