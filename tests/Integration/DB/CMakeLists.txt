add_executable(AtlasNet_Integration_DB
    Entry.cpp
)

target_link_libraries(AtlasNet_Integration_DB PRIVATE
    AtlasNetCore
    AtlasNetDB
    GTest::gtest
)

add_dependencies(AtlasNet_Integration_DB
    Valkey-Server-Compile
)

target_compile_definitions(AtlasNet_Integration_DB PRIVATE
    ATLAS_TEST_VALKEY_SERVER="$<TARGET_FILE:Valkey-Server-Debug>"
    ATLAS_TEST_DB_MODULE="$<TARGET_FILE:AtlasNetDB>"
)

gtest_discover_tests(AtlasNet_Integration_DB)
target_compile_definitions(AtlasNet_Integration_DB PRIVATE
    ATLAS_TEST_VALKEY_SERVER="$<TARGET_FILE:Valkey-Server-Debug>"
    ATLAS_TEST_DB_MODULE="$<TARGET_FILE:AtlasNetDB>"
)
