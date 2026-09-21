add_executable(AtlasNet_tests_Node
    Node.cpp
    ${PROJECT_SOURCE_DIR}/src/Node/AtlasNetNode.cpp
    ${PROJECT_SOURCE_DIR}/src/Standalone/Configuration.cpp
)
target_include_directories(AtlasNet_tests_Node PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(AtlasNet_tests_Node PRIVATE AtlasNetCore GTest::gtest_main)
gtest_discover_tests(AtlasNet_tests_Node)
