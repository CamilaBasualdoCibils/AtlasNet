
add_executable(AtlasNet_tests_Messages ${CMAKE_CURRENT_SOURCE_DIR}/MessageLocal.cpp)

target_link_libraries(AtlasNet_tests_Messages
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)

gtest_discover_tests(AtlasNet_tests_Messages)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Messages_TESTS} PROPERTIES LABELS "MessageLocal")