
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
//#include "atlasnet/core/spatial/SpatialQuery.hpp"
//#include "atlasnet/core/spatial/SpatialQueryCore.hpp"
#include <gtest/gtest.h>
#include <unordered_map>
using namespace AtlasNet;
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}/* 
ATLASNET_SQUERY(MySimpleQuery3DRay, Cartesian3D, RayTag);

ATLASNET_SQUERY(MySimpleQuery2DAABB, Cartesian2D, AABBTag);

ATLASNET_SQUERY(MySimpleQuery2DRadius, Cartesian2D, RadiusTag);

// Custom filter
ATLASNET_DECLARE_SQUERY_FILTER(
    MyGeoQueryFilter,
    std::tuple<int, float>,
    [](AtlasNet::Spatial::QueryCtx& ctx,
       AtlasNet::Spatial::QueryRet& ret,
       int colorFilter,
       float a)
    {
      (void)colorFilter;
      (void)a;

      for (auto& hit : ctx.GetHits())
      {
        if (true)
        {
          ret.pass(hit);
        }
      }
    });

ATLASNET_SQUERY_WITH_FILTER(MyGeoQuery, Geo3D, LineTag, MyGeoQueryFilter);

class FakeSpatialQueryBackend
{
public:
  template <typename TShape>
  std::future<std::vector<Spatial::SpatialQueryHit>> Query(const TShape& shape)
  {
    (void)shape;

    return std::async(std::launch::async, []()
                      {
                        std::vector<Spatial::SpatialQueryHit> hits;
                        hits.push_back({1});
                        hits.push_back({2});
                        hits.push_back({3});
                        return hits;
                      });
  }
};
TEST(Spatial, PointAABB2D) {} */