cmake_minimum_required(VERSION 3.10)

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_CXX_COMPILER clang++)
project(AtlasNet  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 ")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 ")

set (CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(cmake/FetchDeps.cmake)

set(ATLASNET_EXAMPLES ON CACHE BOOL "Build AtlasNet examples")
set(ATLASNET_TESTS ON CACHE BOOL "Build AtlasNet tests")
set(ATLASNET_DEV ON CACHE BOOL "Build AtlasNet dev")
set(ATLASNET_CARTOGRAPH ON CACHE BOOL "Build AtlasNet web interface")
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUG>
    ATLASNET_DB_DEBUG_HANDSHAKE_PORT=5000
)

add_custom_target(AtlasNetDocs
    COMMAND doxygen docs/Doxyfile
    COMMAND xdg-open docs/html/index.html
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating documentation with Doxygen"
    VERBATIM
)

if (ATLASNET_EXAMPLES)
    add_subdirectory(examples)
endif()

if (ATLASNET_TESTS)
enable_testing()
    add_subdirectory(tests)
endif()

if (ATLASNET_DEV)
    add_subdirectory(dev)
endif()

# ========= ATLASNET CORE ========
file(GLOB_RECURSE _core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Core/*.cpp"
)

add_library(AtlasNetCore STATIC ${_core_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetCore
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Core"    

)
target_compile_definitions(AtlasNetCore PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

target_link_libraries(AtlasNetCore
PUBLIC
  Boost::beast
  Boost::container
  Boost::describe
  Boost::geometry
  Boost::graph
  Boost::multi_index
  Boost::stacktrace_addr2line
  Boost::static_string
  Boost::type_traits
  Boost::uuid
  Boost::program_options
  spdlog::spdlog
  Bitsery::bitsery
  GameNetworkingSockets::GameNetworkingSockets_s
  glm::glm
  redis++::redis++_static
  hiredis::hiredis
  hiredis::hiredis_ssl
  $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
  nlohmann_json::nlohmann_json
)
set_target_properties(AtlasNetCore PROPERTIES FOLDER "AtlasNet")

# ========= ATLASNET Node ========
file(GLOB_RECURSE _node_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Node/*.cpp"
)

add_executable(AtlasNetNode ${_node_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetNode
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"

)

target_link_libraries(AtlasNetNode AtlasNetCore)
set_target_properties(AtlasNetNode PROPERTIES FOLDER "AtlasNet")

# ========= ATLASNET DB ========
file(GLOB_RECURSE _db_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/DB/*.cpp"
)

add_library(AtlasNetDB SHARED ${_db_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetDB
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/external/valkey/src"

)
target_link_libraries(AtlasNetDB AtlasNetCore)
set_target_properties(AtlasNetDB PROPERTIES FOLDER "AtlasNet")

# DB Debug
add_custom_target(Valkey-Server-Compile
    COMMAND make -j${NPROC} valkey-server
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/external/valkey
    COMMENT "Compiling Valkey Server for AtlasNetDB debugging"
    VERBATIM
)
add_executable(Valkey-Server-Debug IMPORTED GLOBAL)

set_target_properties(Valkey-Server-Debug PROPERTIES
    IMPORTED_LOCATION
        "${CMAKE_SOURCE_DIR}/external/valkey/src/valkey-server"
)

add_dependencies(
    Valkey-Server-Debug
    Valkey-Server-Compile
    AtlasNetDB
)

# ========= ATLASNET CARTOGRAPH ========
if (ATLASNET_CARTOGRAPH)
  include(cmake/CartographFetchDeps.cmake)
      #directory(web)

  file(GLOB_RECURSE _cartograph_backend_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Cartograph/Backend/*.cpp"
  )

  add_executable(AtlasNetCartographBackend ${_cartograph_backend_sources})
  # Optional, but common: local includes
  target_include_directories(AtlasNetCartographBackend
        PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/Cartograph/Backend"
  )
  target_link_libraries(AtlasNetCartographBackend
  AtlasNetCore
  Drogon::Drogon
  )
  target_compile_definitions(AtlasNetCartographBackend PRIVATE ATLASNET_CARTOGRAPH_BACKEND_BUILD)
  set_target_properties(AtlasNetCartographBackend PROPERTIES FOLDER "AtlasNet")

  add_custom_target(AtlasNetCartographFrontend_DevRun
      COMMAND npm install
      COMMAND npm run dev
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/Cartograph/Frontend
      COMMENT "Developing AtlasNet Cartograph Frontend"
      VERBATIM
  )
  add_custom_target(AtlasNetCartographFrontend_Clean
      COMMAND npm run clean
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/Cartograph/Frontend
      COMMENT "Cleaning AtlasNet Cartograph Frontend"
      VERBATIM
  )
endif()

# ========= ATLASNET Server API ========
file(GLOB_RECURSE _API_Server_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Server/*.cpp"
)
add_library(AtlasNetServerAPI STATIC ${_API_Server_sources})
add_library(AtlasNetServerAPI_s SHARED ${_API_Server_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetServerAPI
      PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Server"

)
target_include_directories(AtlasNetServerAPI_s
      PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Server"

)
target_link_libraries(AtlasNetServerAPI AtlasNetCore)
target_link_libraries(AtlasNetServerAPI_s AtlasNetCore)
set_target_properties(AtlasNetServerAPI PROPERTIES FOLDER "AtlasNet/SDK")
set_target_properties(AtlasNetServerAPI_s PROPERTIES FOLDER "AtlasNet/SDK")

# ========= ATLASNET CLIENT API ========
file(GLOB_RECURSE _API_client_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Client/*.cpp"
)
add_library(AtlasNetClientAPI_s STATIC ${_API_client_sources})
add_library(AtlasNetClientAPI SHARED ${_API_client_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetClientAPI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Client"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(AtlasNetClientAPI_s
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SDK/Client"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(AtlasNetClientAPI AtlasNetCore)
target_link_libraries(AtlasNetClientAPI_s AtlasNetCore)
set_target_properties(AtlasNetClientAPI PROPERTIES FOLDER "AtlasNet/SDK")
set_target_properties(AtlasNetClientAPI_s PROPERTIES FOLDER "AtlasNet/SDK")
