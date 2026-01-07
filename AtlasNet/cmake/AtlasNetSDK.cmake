

# Path to your SDK folder
project(AtlasNet LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ATLASNET_ROOT ${CMAKE_CURRENT_LIST_DIR})


include (${CMAKE_CURRENT_LIST_DIR}/cmake/cmake/Definitions.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/cmake/cmake/Install.cmake)
set(SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/sdk")
set(DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/deps")

# Global link
link_directories(${CMAKE_SOURCE_DIR}/third_party/lib)

# Recursively find all CMakeLists.txt
file(GLOB_RECURSE SDK_CMAKELISTS RELATIVE ${SDK_DIR} "${SDK_DIR}/*/CMakeLists.txt")

foreach(cmake_file IN LISTS SDK_CMAKELISTS)
    # get the directory containing the CMakeLists.txt
    get_filename_component(subdir "${cmake_file}" DIRECTORY)
    message(STATUS "Adding SDK subdirectory: ${subdir}")
    add_subdirectory("${SDK_DIR}/${subdir}")
endforeach()