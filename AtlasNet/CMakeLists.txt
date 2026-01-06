cmake_minimum_required(VERSION 3.24)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        message("VCPKG TOOLCHAIN")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "vcpkg toolchain file")
    endif()
endif()

# Always set install dir (do not hide behind the toolchain fallback)
set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed"
    CACHE PATH "vcpkg installed dir")

project(AtlasNet LANGUAGES C CXX)

# --- Node.js conditional flag ---
option(ENABLE_NODE "Enable Node.js build for Cartograph" OFF)

# --- SWIG and Node.js setup (conditional) ---
if(ENABLE_NODE)
    message(STATUS "Node.js integration enabled")

    set(NODE_PROJECT_DIR apps/Cartograph/web)

    execute_process(
        COMMAND node -p "process.execPath"
        OUTPUT_VARIABLE NODE_EXEC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NODE_EXEC STREQUAL "")
        message(FATAL_ERROR "Node.js not found but ENABLE_NODE=ON")
    endif()

    get_filename_component(NODE_BIN_DIR "${NODE_EXEC}" DIRECTORY)
    get_filename_component(NODE_ROOT_DIR "${NODE_BIN_DIR}" DIRECTORY)

    set(NODE_INCLUDE_DIR "${NODE_ROOT_DIR}/include/node")

    if (NOT EXISTS "${NODE_INCLUDE_DIR}/node_api.h")
        message(FATAL_ERROR
            "Node headers not found at ${NODE_INCLUDE_DIR}\n"
            "Make sure Node is installed with development headers"
        )
    endif()

    message(STATUS "Node include dir = ${NODE_INCLUDE_DIR}")

    # Full native build only if Node is enabled
    add_subdirectory(apps/Cartograph) # HAS TO GO FIRST DUE TO NODEJS SWIG REASONS

    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${NODE_PROJECT_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_INCLUDE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\"" "" NODE_ADDON_API_INCLUDE "${NODE_ADDON_API_INCLUDE}")
    include(cmake/AddSwigLibrary.cmake)
    add_subdirectory(libs/Web)

endif()
include(GNUInstallDirs)


# Root of AtlasNet installation
set(ATLASNET_INSTALL_ROOT ${CMAKE_INSTALL_PREFIX})
set(ATLASNET_INSTALL_BIN     bin)
set(ATLASNET_INSTALL_SDK     sdk)
set(ATLASNET_INSTALL_DOCKER  docker)
set(ATLASNET_INSTALL_SCHEMA  schemas)
set(ATLASNET_INSTALL_CMAKE   cmake)

# Override for debug build (install locally)
if(TRUE)
    message(STATUS "Debug build: installing to local ./bin instead of system prefix")
    set(ATLASNET_INSTALL_ROOT ${CMAKE_SOURCE_DIR}/package)
    set(ATLASNET_INSTALL_BIN     ${ATLASNET_INSTALL_ROOT}/bin)
    set(ATLASNET_INSTALL_SDK     ${ATLASNET_INSTALL_ROOT}/sdk)
    set(ATLASNET_INSTALL_RUNTIME_APPS  ${ATLASNET_INSTALL_ROOT}/runtime)
    set(ATLASNET_INSTALL_SCHEMA  ${ATLASNET_INSTALL_ROOT}/schemas)
    set(ATLASNET_INSTALL_CMAKE   ${ATLASNET_INSTALL_ROOT}/cmake)
endif()

SET(ATLASNET_INSTALL_BOOTSTRAP_CMD AtlasNetBootstrap)
SET(ATLASNET_INSTALL_INDOCKER_CMD AtlasNetDocker)

install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/libs/
    DESTINATION ${ATLASNET_INSTALL_SDK}   # relative to CMAKE_INSTALL_PREFIX
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cpp"
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.txt"
)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/apps/
    DESTINATION ${ATLASNET_INSTALL_RUNTIME_APPS}   # relative to CMAKE_INSTALL_PREFIX
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cpp"
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.txt"
        PATTERN "*.js"
        PATTERN "*.ts"
        PATTERN "*.tsx"
        PATTERN "*.svg"
        PATTERN "*.json"
        PATTERN "*.mjs"

)
install(
    FILES ${CMAKE_SOURCE_DIR}/CMakeLists.txt ${CMAKE_SOURCE_DIR}/vcpkg.json
    DESTINATION ${ATLASNET_INSTALL_CMAKE}
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)

# Copy the entire ./cmake folder
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/
    DESTINATION ${ATLASNET_INSTALL_CMAKE}/cmake
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cmake"
        PATTERN "CMakeLists.txt"  # if you have any inside cmake/
)
# --- Global definitions, compile options, etc ---
set(GLOBAL_DEFINITIONS
    _PORT_WATCHDOG=25564
    _PORT_SHARD=25565
    _PORT_GAMESERVER=25566
    _PORT_PROXY=25568
    _DOCKER_OS_="ubuntu:24.04"
    _DOCKER_WORKDIR_="/atlasnet"
    _ATLASNET_STACK_NAME="atlasnet"
    _SWARM_DEFAULT_PORT=2377
    _DOCKER_TEMP_FILES_DIR="temp/docker/"
    _ATLASNET_NETWORK_NAME="AtlasNet"
    _REGISTRY_SERVICE_NAME="registry"
    _REGISTRY_PORT=5000

    _HEALTH_PING_INTERVAL_MS=500
    _HEALTH_CHECK_INTERVAL_MS=2000
    _HEALTH_PING_TIMESTAMP_LIFE_MS=3000

    
    _INTERNAL_REDIS_SERVICE_NAME="InternalDB"
    _INTERNAL_REDIS_PORT=6379
    _BUILTINDB_REDIS_SERVICE_NAME="BuiltInDB_Redis"
    _BUILTINDB_REDIS_PORT=2380
    _BUILTINDB_POSTGRES_SERVICE_NAME="BuiltInDB_PostGres"
    _BUILTINDB_POSTGRES_PORT=5432
    _SHARD_SERVICE_NAME="Shard"
    _SHARD_IMAGE_NAME="shard"
    _WATCHDOG_IMAGE_NAME="watchdog"
    _WATCHDOG_SERVICE_NAME="WatchDog"
    _WATCHDOG_COMPUTE_HEURISTIC_INTERVAL_S=20
    _PROXY_IMAGE_NAME="proxy"
    _PROXY_SERVICE_NAME="Proxy"
    _CARTOGRAPH_IMAGE_NAME="cartograph"
    _CARTOGRAPH_SERVICE_NAME="Cartograph"
    _GAME_SERVER_IMAGE_NAME="shard_game_server"
    _REGISTRY_CERT_SECRET_NAME="registry_tls_cert"
    _REGISTRY_KEY_SECRET_NAME="registry_tls_key"
    _BUILDER_CONTAINER_NAME="atlasnet_builder"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
foreach(def IN LISTS GLOBAL_DEFINITIONS)
    add_compile_definitions(${def})
endforeach()

# --- Add other subdirectories unconditionally ---
add_subdirectory(libs/AtlasNet/Server)
add_subdirectory(libs/AtlasNet/Client)
add_subdirectory(libs/AtlasNet/Global)
add_subdirectory(libs/BuiltInDB)
add_subdirectory(libs/Database)
add_subdirectory(libs/Debug)
add_subdirectory(libs/Docker)
add_subdirectory(libs/Entity)
add_subdirectory(libs/Events)
add_subdirectory(libs/Heuristic)
add_subdirectory(libs/Interlink)
add_subdirectory(libs/InternalDB)
add_subdirectory(apps/WatchDog)
add_subdirectory(apps/Proxy)
add_subdirectory(apps/Shard)
add_subdirectory(apps/AtlasNet)


