include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Fetching mimalloc")
set(MI_BUILD_SHARED OFF CACHE BOOL "Build mimalloc shared library" FORCE)
set(MI_BUILD_STATIC ON CACHE BOOL "Build mimalloc static library" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "Build mimalloc object library" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)
set(MI_OVERRIDE OFF CACHE BOOL "Do not override the process allocator" FORCE)
FetchContent_Declare(
    mimalloc
    URL https://github.com/microsoft/mimalloc/archive/refs/tags/v3.5.3.tar.gz
    USES_TERMINAL_DOWNLOAD TRUE
    DOWNLOAD_NO_EXTRACT FALSE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(mimalloc)
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


