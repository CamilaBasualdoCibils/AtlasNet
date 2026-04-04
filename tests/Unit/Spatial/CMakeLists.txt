




add_executable(SpatialTests ${CMAKE_CURRENT_SOURCE_DIR}/SpatialTests.cpp)
# Optional, but common: local includes

target_link_libraries(SpatialTests
AtlasNetTestCrashUtils
gtest
)

gtest_discover_tests(SpatialTests)

# Optional labels for filtering in ctest
set_tests_properties(${SpatialTests_TESTS} PROPERTIES LABELS "Spatial")
