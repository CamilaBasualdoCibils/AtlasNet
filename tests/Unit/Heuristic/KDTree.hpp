#pragma once
#include "DebugView.hpp"
#include "HeuristicUtils.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "atlasnet/core/heuristic/IHeuristic.hpp"
#include "atlasnet/core/heuristic/KDTree/KDTree.hpp"
#include "raylib.h"
#include <algorithm>
#include <atomic>
#include <execution>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <vector>
namespace AtlasNet::KDTree
{
TEST(KDTree2D, Sphere)
{
  std::vector<vec2> points = TestUtils::HeuristicUtils::GeneratePolarPoints<
      TestUtils::HeuristicTestDim::e2D>(1000, 100.0f);

  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e2D,
                                            KDTreeHeuristic<2, float>,
                                            KDTreePartition<2, float>>(points);
}
TEST(KDTree2D, Line)
{
  std::vector<vec2> points = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e2D>(1000, {-100.f, 0.0f}, {100.0f, 0.0f});

  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e2D,
                                            KDTreeHeuristic<2, float>,
                                            KDTreePartition<2, float>>(points);

  points = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e2D>(500, {-100.0f, -100.0f},
                                        {100.0f, 100.0f});
  std::vector<vec2> points2 = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e2D>(500, {-100.0f, 100.0f},
                                        {100.0f, -100.0f});
  points.insert(points.end(), points2.begin(), points2.end());

  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e2D,
                                            KDTreeHeuristic<2, float>,
                                            KDTreePartition<2, float>>(points);
}
TEST(KDTree3D, Sphere)
{
  std::vector<vec3> points = TestUtils::HeuristicUtils::GeneratePolarPoints<
      TestUtils::HeuristicTestDim::e3D>(1000, 100.0f);

  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e3D,
                                            KDTreeHeuristic<3, float>,
                                            KDTreePartition<3, float>>(points);
}
TEST(KDTree3D, Line)
{
  std::vector<vec3> points = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e3D>(1000, {-100.f, 0.0f, 0.0f},
                                        {100.0f, 0.0f, 0.0f});

                                        points.push_back({0,100.0f,0.0f});
                                        points.push_back({0,-100.0f,0.0f});
                                        points.push_back({0.0f,0.0f,100.0f});
                                        points.push_back({0.0f,0.0f,-100.0f});
  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e3D,
                                            KDTreeHeuristic<3, float>,
                                            KDTreePartition<3, float>>(points);

  points = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e3D>(500, {-100.0f, -100.0f, -100.0f},
                                        {100.0f, 100.0f, 100.0f});
  std::vector<vec3> points2 = TestUtils::HeuristicUtils::GenLinePoints<
      TestUtils::HeuristicTestDim::e3D>(500, {-100.0f, 100.0f, -100.0f},
                                        {100.0f, -100.0f, 100.0f});
  points.insert(points.end(), points2.begin(), points2.end());

  TestUtils::HeuristicUtils::SequentialTest<TestUtils::HeuristicTestDim::e3D,
                                            KDTreeHeuristic<3, float>,
                                            KDTreePartition<3, float>>(points);
}
TEST(KDTree2D, Motion)
{
  TestUtils::HeuristicUtils::MotionTest<TestUtils::HeuristicTestDim::e2D,
                                        KDTreeHeuristic<2, float>,
                                        KDTreePartition<2, float>>(3, 10, 1000);
}
TEST(KDTree2D, MotionHeavy)
{
  TestUtils::HeuristicUtils::MotionTest<TestUtils::HeuristicTestDim::e2D,
                                        KDTreeHeuristic<2, float>,
                                        KDTreePartition<2, float>>(10, 1000,
                                                                   1000);
}
TEST(KDTree3D, Motion)
{
  TestUtils::HeuristicUtils::MotionTest<TestUtils::HeuristicTestDim::e3D,
                                        KDTreeHeuristic<3, float>,
                                        KDTreePartition<3, float>>(3, 10, 1000);
}
TEST(KDTree3D, MotionHeavy)
{
  TestUtils::HeuristicUtils::MotionTest<TestUtils::HeuristicTestDim::e3D,
                                        KDTreeHeuristic<3, float>,
                                        KDTreePartition<3, float>>(10, 1000,
                                                                   1000);
}
} // namespace AtlasNet::KDTree