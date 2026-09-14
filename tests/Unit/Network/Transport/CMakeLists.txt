
add_executable(AtlasNet_tests_Transport 
${CMAKE_CURRENT_SOURCE_DIR}/Transport.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/UDP.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/Packets.cpp)

target_link_libraries(AtlasNet_tests_Transport
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
raylib
)

gtest_discover_tests(AtlasNet_tests_Transport)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Transport_TESTS} PROPERTIES LABELS "Transport")