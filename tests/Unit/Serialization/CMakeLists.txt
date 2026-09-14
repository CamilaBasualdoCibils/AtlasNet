




add_executable(AtlasNet_tests_Serialization ${CMAKE_CURRENT_SOURCE_DIR}/Serialization.cpp)
# Optional, but common: local includes

target_link_libraries(AtlasNet_tests_Serialization
AtlasNetCore
GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)

gtest_discover_tests(AtlasNet_tests_Serialization)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Serialization_TESTS} PROPERTIES LABELS "Serialization")
