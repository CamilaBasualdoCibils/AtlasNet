cmake_minimum_required(VERSION 3.20)
project(AtlasUnityBridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# --------------------------------------------------
# Automatically detect project root (src directory)
# --------------------------------------------------
# Find the directory two levels above this file
get_filename_component(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

message(STATUS "Detected SRC_ROOT: ${SRC_ROOT}")

# --------------------------------------------------
# Gather all relevant source files automatically
# --------------------------------------------------
file(GLOB_RECURSE SRC_FILES
    "${SRC_ROOT}/AtlasNet/Server/*.cpp"
    "${SRC_ROOT}/AtlasNet/*.cpp"
    "${SRC_ROOT}/Partition/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Filter out files that contain "Interface" or "ImGui"
foreach(file ${SRC_FILES})
    if(file MATCHES "Interface" OR file MATCHES "ImGui")
        list(REMOVE_ITEM SRC_FILES ${file})
    endif()
endforeach()

if(NOT SRC_FILES)
    message(FATAL_ERROR "No source files found in ${SRC_ROOT}")
endif()

# --------------------------------------------------
# Include directories dynamically
# --------------------------------------------------
include_directories(
    ${SRC_ROOT}
    ${SRC_ROOT}/AtlasNet/Server
    ${SRC_ROOT}/AtlasNet
    ${SRC_ROOT}/Partition
    ${SRC_ROOT}/
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# --------------------------------------------------
# Build the shared library
# --------------------------------------------------
add_library(AtlasUnityBridge SHARED ${SRC_FILES})

set_target_properties(AtlasUnityBridge PROPERTIES
    OUTPUT_NAME "AtlasUnityBridge"
)

# --------------------------------------------------
# Windows compile settings
# --------------------------------------------------
if(MSVC)
    target_compile_definitions(AtlasUnityBridge PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
