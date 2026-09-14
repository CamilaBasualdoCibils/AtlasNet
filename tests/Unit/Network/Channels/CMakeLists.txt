
add_executable(AtlasNet_tests_Channels
${CMAKE_CURRENT_SOURCE_DIR}/Channels.cpp)

target_link_libraries(AtlasNet_tests_Channels
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
raylib
)


gtest_discover_tests(AtlasNet_tests_Channels)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Channels_TESTS} PROPERTIES LABELS "Channels")