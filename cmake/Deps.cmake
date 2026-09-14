include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Fetching GLM")
SET(GLM_BUILD_TESTS OFF)
FetchContent_Declare(
  glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(glm)
message(STATUS "Fetching Boost")
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Use static Boost libraries" FORCE)
set(BOOST_INCLUDE_LIBRARIES beast bimap describe graph dynamic_bitset flyweight math multi_array multi_index lockfree stacktrace serialization static_string uuid program_options)
set(BOOST_ENABLE_MPI ON)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.gz
        #GIT_REPOSITORY https://github.com/boostorg/boost.git
        #GIT_TAG boost-1.91.0-1
        USES_TERMINAL_DOWNLOAD TRUE
        DOWNLOAD_NO_EXTRACT FALSE
      )
FetchContent_MakeAvailable(Boost)
# --- Fetch Libuv ---
message(STATUS "Fetching libuv")

FetchContent_Declare(
  libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG v1.48.0
)

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libuv)
# --- Fetch hiredis ---
find_package(OpenSSL REQUIRED)
message(STATUS "Fetching Redis-Plus-Plus")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Hiredis as static" FORCE)
set(ENABLE_SSL ON CACHE BOOL "Enable SSL support in hiredis" FORCE)
set(DISABLE_TESTS ON CACHE BOOL "Disable building hiredis tests" FORCE)
FetchContent_Declare(
  hiredis
  URL https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
  SOURCE_DIR _deps/hiredis
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(hiredis)

set(REDIS_PLUS_PLUS_BUILD_ASYNC "libuv")
set(REDIS_PLUS_PLUS_BUILD_STATIC ON CACHE BOOL "Build Redis-Plus-Plus static" FORCE)
#set(REDIS_PLUS_PLUS_BUILD_ASYNC "libuv")
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  redis-plus-plus
  URL https://github.com/sewenew/redis-plus-plus/archive/refs/tags/1.3.15.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(redis-plus-plus)

FetchContent_Declare(
  etcd-cpp-apiv3
  GIT_REPOSITORY https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3.git
)
FetchContent_MakeAvailable(etcd-cpp-apiv3)
# Only populate if not already done
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    Protobuf
    URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v21.2.tar.gz
)
FetchContent_GetProperties(Protobuf)
if(NOT Protobuf_POPULATED)
    FetchContent_Populate(Protobuf)
    add_subdirectory(${protobuf_SOURCE_DIR} ${protobuf_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Make sure GameNetworkingSockets sees it
set(Protobuf_INCLUDE_DIR ${protobuf_SOURCE_DIR}/src CACHE PATH "Protobuf include dir")
set(Protobuf_LIBRARIES protobuf::libprotobuf CACHE STRING "Protobuf library")
set(Protobuf_PROTOC_EXECUTABLE ${protobuf_BINARY_DIR}/protoc CACHE FILEPATH "Protoc executable")
set(Protobuf_USE_STATIC_LIBS ON CACHE BOOL "")

# Now FetchContent GNS
message(STATUS "Fetching GameNetworkingSockets")
FetchContent_Declare(
    GameNetworkingSockets
    URL https://github.com/ValveSoftware/GameNetworkingSockets/archive/refs/tags/v1.4.1.tar.gz
    USES_TERMINAL_DOWNLOAD TRUE
    DOWNLOAD_NO_EXTRACT FALSE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build GameNetworkingSockets as static" FORCE)
FetchContent_MakeAvailable(GameNetworkingSockets)



message(STATUS "TaskFlow")
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(taskflow 
URL https://github.com/taskflow/taskflow/archive/refs/tags/v4.0.0.tar.gz
)
FetchContent_MakeAvailable(taskflow)


message(STATUS "Fetching spdlog")
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
    USES_TERMINAL_DOWNLOAD TRUE
    #GIT_TAG v1.17.0
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)
# Fetch Kokkos
# set (Kokkos_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
# FetchContent_Declare(
#     kokkos
#     URL https://github.com/kokkos/kokkos/archive/refs/tags/5.1.0.tar.gz
# )
# 
# FetchContent_MakeAvailable(kokkos)


FetchContent_Declare(
    ENTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.16.0.tar.gz
)
FetchContent_MakeAvailable(ENTT)

message(STATUS "Nlohmann Json")
FetchContent_Declare(nlohmann_json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(nlohmann_json)
include(FetchContent)

Set(BUILD_DOC OFF CACHE BOOL "" FORCE)
Set(BUILD_SANDBOX OFF CACHE BOOL "" FORCE)
Set(SKIP_PERFORMANCE_COMPARISON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  Cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  GIT_TAG v1.3.2
)
FetchContent_MakeAvailable(Cereal)

FetchContent_Declare(
  bitsery
  GIT_REPOSITORY https://github.com/fraillt/bitsery.git
  
)
FetchContent_MakeAvailable(bitsery)