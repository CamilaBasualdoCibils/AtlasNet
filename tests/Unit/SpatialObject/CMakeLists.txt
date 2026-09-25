add_executable(AtlasNet_tests_SpatialObject
  ${CMAKE_CURRENT_SOURCE_DIR}/SpatialObjectTests.cpp)

target_link_libraries(AtlasNet_tests_SpatialObject
  AtlasNetCore
  GTest::gtest
  GTest::gtest_main
  GTest::gmock
  GTest::gmock_main
  EnTT::EnTT
)

set_target_properties(AtlasNet_tests_SpatialObject PROPERTIES
  FOLDER "AtlasNet/Tests/Unit/SpatialObject"
)

gtest_discover_tests(AtlasNet_tests_SpatialObject
  PROPERTIES LABELS "SpatialObject"
)
