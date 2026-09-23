add_library(AtlasNetTestShardModule MODULE TestShardModule.cpp)
target_link_libraries(AtlasNetTestShardModule PRIVATE AtlasNetCore AtlasNetModule)
set_target_properties(AtlasNetTestShardModule PROPERTIES PREFIX "")

add_executable(AtlasNet_tests_Shard
    Shard.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/Shard/ShardManager.cpp
)
target_link_libraries(AtlasNet_tests_Shard PRIVATE AtlasNetCore GTest::gtest_main)
target_compile_definitions(AtlasNet_tests_Shard PRIVATE
    ATLASNET_SHARD_WORKER="$<TARGET_FILE:AtlasNetShardWorker>"
    ATLASNET_TEST_SHARD_MODULE="$<TARGET_FILE:AtlasNetTestShardModule>"
)
add_dependencies(AtlasNet_tests_Shard AtlasNetShardWorker AtlasNetTestShardModule)
gtest_discover_tests(AtlasNet_tests_Shard)
