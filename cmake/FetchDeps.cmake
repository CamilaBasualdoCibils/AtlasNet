include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
find_package(Boost REQUIRED CONFIG COMPONENTS
    beast
    container
    describe
    geometry
    graph
    multi_index
    stacktrace_addr2line
    static_string
    type_traits
    uuid
    program_options
)

find_package(spdlog CONFIG REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(hiredis_ssl CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(Bitsery CONFIG REQUIRED)

# EnTT is header-only; scoped for SpatialObject / component backend.
include(FetchContent)
FetchContent_Declare(
  entt
  URL https://github.com/skypjack/entt/archive/refs/tags/v3.16.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(entt)

