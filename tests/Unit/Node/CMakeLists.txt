add_executable(AtlasNet_tests_Node
    Node.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/AtlasNetNode.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/Controller/Controller.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/Shard/ShardManager.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/Module/ModuleLoader.cpp
    ${PROJECT_SOURCE_DIR}/src/Standalone/Configuration.cpp
)
target_include_directories(AtlasNet_tests_Node PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(AtlasNet_tests_Node PRIVATE AtlasNetCore AtlasNetModule GTest::gtest_main)
target_compile_definitions(AtlasNet_tests_Node PRIVATE
    ATLASNET_STEAMNETSOCK_MODULE="$<TARGET_FILE:AtlasNetSteamNetSock>"
)
add_dependencies(AtlasNet_tests_Node AtlasNetSteamNetSock)
gtest_discover_tests(AtlasNet_tests_Node)
