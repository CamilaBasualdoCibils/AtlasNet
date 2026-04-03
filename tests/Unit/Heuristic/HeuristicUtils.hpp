#pragma once

#include "SceneView.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "atlasnet/core/heuristic/IHeuristic.hpp"
#include "raylib.h"

#include <atomic>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace AtlasNet::TestUtils
{

enum class HeuristicTestDim
{
  e2D,
  e3D
};

class HeuristicUtils
{
public:
  template <HeuristicTestDim mode>
  static std::vector<
      std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
  GeneratePolarPoints(int pointCount, float Maxradius);

  template <HeuristicTestDim mode>
  static std::vector<
      std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
  GenLinePoints(
      int pointCount,
      std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3> from,
      std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3> to);

  template <HeuristicTestDim mode, typename TreeHeuristicType,
            typename TreePartitionType>
  static void SequentialTest(
      std::span<std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
          pointsSource,
      uint32_t MaxRegions = 100);

  template <HeuristicTestDim mode, typename TreeHeuristicType,
            typename TreePartitionType>
  static void MotionTest(uint32_t regionCount = 10, uint32_t pointCount = 1000,
                         uint32_t stepCount = 100);
};

template <HeuristicTestDim mode, typename TreeHeuristicType,
          typename TreePartitionType>
inline void HeuristicUtils::MotionTest(uint32_t regionCount,
                                       uint32_t pointCount,
                                       uint32_t stepCount)
{
  using Is2D = std::bool_constant<mode == HeuristicTestDim::e2D>;
  constexpr size_t Dim = Is2D::value ? 2u : 3u;
  using VecType = std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>;

  using PartitionBase = AtlasNet::IPartitionT<Dim, float>;
  using RegionBase = AtlasNet::IRegionT<Dim, float>;
  using KDRegionType = typename TreePartitionType::RegionImpl;

  float SceneScale = 100.0f;
  AABB<Dim, float> SceneBounds(VecType(-SceneScale), VecType(SceneScale));

  std::vector<VecType> pointsSource(pointCount, VecType(0));
  std::vector<VecType> motionVectors = GeneratePolarPoints<mode>(pointCount, 30.0f);

  std::unordered_map<AtlasNet::RegionID, VecType> previousCentroids;

  TreeHeuristicType tree;
  AtlasNet::SimpleRegionIdGenerator idGen;

  std::unique_ptr<PartitionBase> partition;
  TreePartitionType* kdPartition = nullptr;

  auto partitionFunc = [&](size_t desiredCount)
  {
    partition = tree.Partition(pointsSource, desiredCount, idGen);
    kdPartition = dynamic_cast<TreePartitionType*>(partition.get());
    ASSERT_NE(kdPartition, nullptr);
  };

  auto repartitionFunc = [&](size_t desiredCount)
  {
    for (const auto& region : kdPartition->Regions())
    {
      previousCentroids[region->GetID()] = region->GetCentroid();
    }

    partition = tree.Repartition(pointsSource, desiredCount, *kdPartition, idGen);
    kdPartition = dynamic_cast<TreePartitionType*>(partition.get());
    ASSERT_NE(kdPartition, nullptr);
  };

  partitionFunc(1);

  SceneView view = [&]()
  {
    if constexpr (Is2D::value)
    {
      return SceneView(
          SceneView::Scene2DSettings{.SceneScale = SceneScale * 2.0f});
    }
    else
    {
      return SceneView(
          SceneView::Scene3DSettings{.SceneScale = SceneScale * 2.0f});
    }
  }();

  view.on_update(
      [&](SceneView::Context& ctx)
      {
        const float TimePerSplit = 0.01f;

        for (int i = 2; i <= static_cast<int>(stepCount); ++i)
        {
          ctx.timer(std::format("split_up_{}", i), TimePerSplit * (i - 1))
              .on_first_active(
                  [&]()
                  {
                    for (size_t p = 0; p < pointsSource.size(); ++p)
                    {
                      auto& point = pointsSource[p];

                      for (size_t d = 0; d < Dim; ++d)
                      {
                        if (point[d] < SceneBounds.min[d] ||
                            point[d] > SceneBounds.max[d])
                        {
                          motionVectors[p][d] *= -1.0f;
                        }
                      }

                      point += motionVectors[p] * ctx.deltaTime;
                    }

                    repartitionFunc(regionCount);

                    EXPECT_EQ(kdPartition->RegionCount(), i);

                    std::atomic_uint64_t MaximumPointsTotal = 0;
                    std::atomic_uint64_t MinimumPointsTotal =
                        std::numeric_limits<uint64_t>::max();

                    for (const auto& regionBasePtr : kdPartition->Regions())
                    {
                      const auto* kdRegion =
                          dynamic_cast<const KDRegionType*>(regionBasePtr.get());
                      ASSERT_NE(kdRegion, nullptr);

                      EXPECT_GE(kdRegion->GetPointIndices().size(), 0u);

                      MaximumPointsTotal.store(std::max(
                          MaximumPointsTotal.load(),
                          static_cast<uint64_t>(kdRegion->GetPointIndices().size())));

                      MinimumPointsTotal.store(std::min(
                          MinimumPointsTotal.load(),
                          static_cast<uint64_t>(kdRegion->GetPointIndices().size())));
                    }

                    const float Disparity =
                        static_cast<float>(MaximumPointsTotal.load()) /
                        static_cast<float>(MinimumPointsTotal.load());

                    EXPECT_LE(Disparity, 2.0f);

                    for (const auto& point : pointsSource)
                    {
                      auto regionIdOpt = kdPartition->QueryRegion(point);
                      EXPECT_TRUE(regionIdOpt.has_value());

                      auto closestRegionId = kdPartition->QueryClosestRegion(point);
                      (void)closestRegionId;
                    }
                  });
        }

        const float UpPhaseEndTime =
            TimePerSplit * (static_cast<float>(stepCount) - 1.0f);

        ctx.timer("shutdown", UpPhaseEndTime)
            .on_first_active([&]() { ctx.Shutdown(); });

        for (const auto& point : pointsSource)
        {
          if constexpr (Is2D::value)
          {
            DrawPixelV(Vector2{point[0], point[1]}, ORANGE);
          }
          else
          {
            DrawSphereEx(Vector3{point[0], point[1], point[2]}, 1.0f, 1, 3, ORANGE);
          }
        }

        for (const auto& region : kdPartition->Regions())
        {
          const auto regionId = region->GetID();
          const auto& centroid = region->GetCentroid();
          const auto& bounds = region->GetAABB();

          Color lineColor = MAROON;

          if constexpr (Is2D::value)
          {
            std::string text = std::format("{}", regionId);
            int textWidth = MeasureText(text.c_str(), 1);

            DrawText(text.c_str(),
                     static_cast<int>(centroid[0]) - textWidth / 2,
                     static_cast<int>(centroid[1]),
                     1,
                     DARKGRAY);

            if (previousCentroids.contains(regionId))
            {
              DrawLineEx(Vector2{centroid[0], centroid[1]},
                         Vector2{previousCentroids[regionId][0],
                                 previousCentroids[regionId][1]},
                         2.0f,
                         lineColor);
            }
          }
          else
          {
            if (previousCentroids.contains(regionId))
            {
              DrawLine3D(Vector3{centroid[0], centroid[1], centroid[2]},
                         Vector3{previousCentroids[regionId][0],
                                 previousCentroids[regionId][1],
                                 previousCentroids[regionId][2]},
                         lineColor);
            }
          }

          ctx.DrawAABB(bounds, SceneView::DrawMode::Wireframe, BLUE);
        }
      });

  view.run();
}

template <HeuristicTestDim mode>
inline std::vector<
    std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
HeuristicUtils::GenLinePoints(
    int pointCount,
    std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3> from,
    std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3> to)
{
  using VecType = std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>;

  std::vector<VecType> points;
  points.reserve(pointCount);

  for (int i = 0; i < pointCount; ++i)
  {
    float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
    VecType point = from * (1.0f - t) + to * t;
    points.push_back(point);
  }

  return points;
}

template <HeuristicTestDim mode, typename TreeHeuristicType,
          typename TreePartitionType>
inline void HeuristicUtils::SequentialTest(
    std::span<std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
        pointsSource,
    uint32_t MaxRegions)
{
  using Is2D = std::bool_constant<mode == HeuristicTestDim::e2D>;
  constexpr size_t Dim = Is2D::value ? 2u : 3u;
  using VecType = std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>;

  using PartitionBase = AtlasNet::IPartitionT<Dim, float>;
  using KDRegionType = typename TreePartitionType::RegionImpl;

  float SceneScale = 100.0f;

  std::unordered_map<AtlasNet::RegionID, VecType> previousCentroids;

  TreeHeuristicType tree;
  AtlasNet::SimpleRegionIdGenerator idGen;

  std::unique_ptr<PartitionBase> partition;
  TreePartitionType* kdPartition = nullptr;

  auto partitionFunc = [&](size_t regionCount)
  {
    partition = tree.Partition(pointsSource, regionCount, idGen);
    kdPartition = dynamic_cast<TreePartitionType*>(partition.get());
    ASSERT_NE(kdPartition, nullptr);
  };

  auto repartitionFunc = [&](size_t regionCount)
  {
    for (const auto& region : kdPartition->Regions())
    {
      previousCentroids[region->GetID()] = region->GetCentroid();
    }

    partition = tree.Repartition(pointsSource, regionCount, *kdPartition, idGen);
    kdPartition = dynamic_cast<TreePartitionType*>(partition.get());
    ASSERT_NE(kdPartition, nullptr);
  };

  partitionFunc(1);

  SceneView view = [&]()
  {
    if constexpr (Is2D::value)
    {
      return SceneView(
          SceneView::Scene2DSettings{.SceneScale = SceneScale * 2.0f});
    }
    else
    {
      return SceneView(
          SceneView::Scene3DSettings{.SceneScale = SceneScale * 2.0f});
    }
  }();

  view.on_update(
      [&](SceneView::Context& ctx)
      {
        const float TimePerSplit = 0.01f;

        for (int i = 2; i <= static_cast<int>(MaxRegions); ++i)
        {
          ctx.timer(std::format("split_up_{}", i), TimePerSplit * (i - 1))
              .on_first_active(
                  [&]()
                  {
                    std::cerr << "Repartitioned to " << i
                              << " regions at time " << ctx.totalTime << std::endl;

                    repartitionFunc(i);

                    EXPECT_EQ(kdPartition->RegionCount(), i);

                    std::atomic_uint64_t MaximumPointsTotal = 0;
                    std::atomic_uint64_t MinimumPointsTotal =
                        std::numeric_limits<uint64_t>::max();

                    for (const auto& regionBasePtr : kdPartition->Regions())
                    {
                      const auto* kdRegion =
                          dynamic_cast<const KDRegionType*>(regionBasePtr.get());
                      ASSERT_NE(kdRegion, nullptr);

                      EXPECT_GE(kdRegion->GetPointIndices().size(), 0u);

                      MaximumPointsTotal.store(std::max(
                          MaximumPointsTotal.load(),
                          static_cast<uint64_t>(kdRegion->GetPointIndices().size())));

                      MinimumPointsTotal.store(std::min(
                          MinimumPointsTotal.load(),
                          static_cast<uint64_t>(kdRegion->GetPointIndices().size())));
                    }

                    const float Disparity =
                        static_cast<float>(MaximumPointsTotal.load()) /
                        static_cast<float>(MinimumPointsTotal.load());

                    EXPECT_LE(Disparity, 2.0f);

                    for (const auto& point : pointsSource)
                    {
                      auto regionIdOpt = kdPartition->QueryRegion(point);
                      EXPECT_TRUE(regionIdOpt.has_value());

                      auto closestRegionId = kdPartition->QueryClosestRegion(point);
                      (void)closestRegionId;
                    }
                  });
        }

        const float UpPhaseEndTime =
            TimePerSplit * (static_cast<float>(MaxRegions) - 1.0f);
        const float DownPhaseStartTime = UpPhaseEndTime + TimePerSplit;

        for (int i = static_cast<int>(MaxRegions) - 1; i >= 1; --i)
        {
          const float t =
              UpPhaseEndTime + TimePerSplit * static_cast<float>(MaxRegions - i);

          ctx.timer(std::format("split_down_{}", i), t)
              .on_first_active(
                  [&]()
                  {
                    std::cerr << "Repartitioned to " << i
                              << " regions at time " << ctx.totalTime << std::endl;
                    repartitionFunc(i);
                    EXPECT_EQ(kdPartition->RegionCount(), i);
                  });
        }

        ctx.timer("shutdown", UpPhaseEndTime + DownPhaseStartTime)
            .on_first_active([&]() { ctx.Shutdown(); });

        for (const auto& point : pointsSource)
        {
          if constexpr (Is2D::value)
          {
            DrawPixelV(Vector2{point[0], point[1]}, ORANGE);
          }
          else
          {
            DrawSphereEx(Vector3{point[0], point[1], point[2]}, 1.0f, 1, 3, ORANGE);
          }
        }

        for (const auto& region : kdPartition->Regions())
        {
          const auto regionId = region->GetID();
          const auto& centroid = region->GetCentroid();
          const auto& bounds = region->GetAABB();

          Color lineColor = MAROON;

          if constexpr (Is2D::value)
          {
            std::string text = std::format("{}", regionId);
            int textWidth = MeasureText(text.c_str(), 1);

            DrawText(text.c_str(),
                     static_cast<int>(centroid[0]) - textWidth / 2,
                     static_cast<int>(centroid[1]),
                     1,
                     DARKGRAY);

            if (previousCentroids.contains(regionId))
            {
              DrawLineEx(Vector2{centroid[0], centroid[1]},
                         Vector2{previousCentroids[regionId][0],
                                 previousCentroids[regionId][1]},
                         2.0f,
                         lineColor);
            }
          }
          else
          {
            if (previousCentroids.contains(regionId))
            {
              DrawLine3D(Vector3{centroid[0], centroid[1], centroid[2]},
                         Vector3{previousCentroids[regionId][0],
                                 previousCentroids[regionId][1],
                                 previousCentroids[regionId][2]},
                         lineColor);
            }
          }

          ctx.DrawAABB(bounds, SceneView::DrawMode::Wireframe, BLUE);
        }
      });

  view.run();
}

template <HeuristicTestDim mode>
inline std::vector<
    std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>>
HeuristicUtils::GeneratePolarPoints(int pointCount, float Maxradius)
{
  using Is2D = std::bool_constant<mode == HeuristicTestDim::e2D>;
  using VecType = std::conditional_t<mode == HeuristicTestDim::e2D, vec2, vec3>;

  std::vector<VecType> points;
  points.reserve(pointCount);

  for (int i = 0; i < pointCount; ++i)
  {
    float RandDist = std::sqrt(static_cast<float>(rand()) / RAND_MAX);
    float RandAngle =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265f;
    float Radius = RandDist * Maxradius;

    float x = std::cos(RandAngle) * Radius;
    float y = std::sin(RandAngle) * Radius;

    if constexpr (Is2D::value)
    {
      points.push_back({x, y});
    }
    else
    {
      float RandAngle2 =
          static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265f;
      float z = std::cos(RandAngle2) * Radius;
      points.push_back({x, y, z});
    }
  }

  return points;
}

} // namespace AtlasNet::TestUtils