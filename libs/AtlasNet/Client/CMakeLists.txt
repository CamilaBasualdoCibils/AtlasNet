cmake_minimum_required(VERSION 3.16)

# Build AtlasNetClient as a shared library (.so/.dll/.dylib) when ON, static (.a/.lib) when OFF
option(ATLASNETCLIENT_SHARED "Build AtlasNetClient as a shared library" OFF)

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(ATLASNETCLIENT_SHARED)
  add_library(AtlasNetClient SHARED ${_sources})
else()
  add_library(AtlasNetClient STATIC ${_sources})
endif()

target_link_libraries(AtlasNetClient PUBLIC AtlasNetGeneral)

target_include_directories(AtlasNetClient PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
