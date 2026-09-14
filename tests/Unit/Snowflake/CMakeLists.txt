




add_executable(AtlasNet_tests_Snowflake ${CMAKE_CURRENT_SOURCE_DIR}/SnowflakeTests.cpp)
# Optional, but common: local includes

target_link_libraries(AtlasNet_tests_Snowflake
AtlasNetCore
gtest
)

gtest_discover_tests(AtlasNet_tests_Snowflake)

# Optional labels for filtering in ctest
set_tests_properties(${AtlasNet_tests_Snowflake_TESTS} PROPERTIES LABELS "Snowflake")
