
add_executable(AtlasNet_tests_IntentChannel
${CMAKE_CURRENT_SOURCE_DIR}/IntentChannel.cpp)

target_link_libraries(AtlasNet_tests_IntentChannel
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
raylib
)


gtest_discover_tests(AtlasNet_tests_IntentChannel)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_IntentChannel_TESTS} PROPERTIES LABELS "IntentChannel")