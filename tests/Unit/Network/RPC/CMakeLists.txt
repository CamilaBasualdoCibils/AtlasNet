
add_executable(AtlasNet_tests_RPC ${CMAKE_CURRENT_SOURCE_DIR}/MockRPC.cpp)

target_link_libraries(AtlasNet_tests_RPC
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)

gtest_discover_tests(AtlasNet_tests_RPC)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_RPC_TESTS} PROPERTIES LABELS "RPC")