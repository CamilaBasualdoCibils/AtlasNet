
add_executable(AtlasNet_tests_Topology ${CMAKE_CURRENT_SOURCE_DIR}/Topology.cpp)

target_link_libraries(AtlasNet_tests_Topology
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
raylib glfw
)

gtest_discover_tests(AtlasNet_tests_Topology)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Topology_TESTS} PROPERTIES LABELS "Topology")