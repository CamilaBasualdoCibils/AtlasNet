cmake_minimum_required(VERSION 3.10)
include(cmake/Deps.cmake)
project(AtlasNet)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")


set(ATLASNET_EXAMPLES ON CACHE BOOL "Build AtlasNet examples")
set(ATLASNET_TESTS ON CACHE BOOL "Build AtlasNet tests")
set(ATLASNET_DEV ON CACHE BOOL "Build AtlasNet dev")

if (ATLASNET_EXAMPLES)
    add_subdirectory(examples)
endif()

if (ATLASNET_TESTS)
    add_subdirectory(tests)
endif()

if (ATLASNET_DEV)
    add_subdirectory(dev)
endif()





# Recursively collect sources under ./src
file(GLOB_RECURSE _core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)

add_library(AtlasNetCore ${_core_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetCore
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)



file(GLOB_RECURSE _proxy_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/proxy/*.cpp"
)

add_executable(AtlasNetProxy ${_proxy_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetProxy
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proxy"
)
add_dependencies(AtlasNetProxy AtlasNetCore)



file(GLOB_RECURSE _watchdog_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/watchdog/*.cpp"
)

add_executable(AtlasNetWatchdog ${_watchdog_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetWatchdog
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/watchdog"
)
add_dependencies(AtlasNetWatchdog AtlasNetCore)


file(GLOB_RECURSE _web_backend_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/web/backend/*.cpp"
)

add_executable(AtlasNetWebBackend ${_web_backend_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetWebBackend
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/web/backend"
)
add_dependencies(AtlasNetWebBackend AtlasNetCore)



file(GLOB_RECURSE _API_server_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/API/server/*.cpp"
)
add_library(AtlasNetServerAPI STATIC ${_API_server_sources})
add_library(AtlasNetServerAPI_s SHARED ${_API_server_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetServerAPI
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/server"
)
add_dependencies(AtlasNetServerAPI AtlasNetCore)

file(GLOB_RECURSE _API_client_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/API/client/*.cpp"
)
add_library(AtlasNetClientAPI STATIC ${_API_client_sources})
add_library(AtlasNetClientAPI_s SHARED ${_API_client_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetClientAPI
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/client"
)
add_dependencies(AtlasNetClientAPI AtlasNetCore)
