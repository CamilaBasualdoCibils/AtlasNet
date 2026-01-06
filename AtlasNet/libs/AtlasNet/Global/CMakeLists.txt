cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME AtlasNetGlobal)

# Use user-defined name if provided, otherwise use folder name
if(NOT DEFINED TARGET_NAME)
    get_filename_component(TARGET_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
endif()

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${TARGET_NAME} ${_sources})

# Optional, but common: local includes
target_include_directories(${TARGET_NAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${NODE_INCLUDE_DIR}
    ${NODE_ADDON_API_INCLUDE}
)

find_package(Boost REQUIRED COMPONENTS container)
find_package(Boost REQUIRED COMPONENTS stacktrace_addr2line)
find_package(Boost REQUIRED COMPONENTS describe)
find_package(Boost REQUIRED COMPONENTS type_traits)
find_package(Boost REQUIRED COMPONENTS uuid)
find_package(Boost REQUIRED COMPONENTS multi_index)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME}
  PUBLIC
    Boost::container
    Boost::stacktrace_addr2line
    Boost::describe
    Boost::type_traits
    Boost::multi_index
    Boost::uuid
    glm::glm
    nlohmann_json::nlohmann_json
)