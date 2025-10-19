cmake_minimum_required(VERSION 3.20)
project(KDNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# --------------------------------------------------
# Directories
# --------------------------------------------------
set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "SRC_ROOT = ${SRC_ROOT}")

# --------------------------------------------------
# Recursively collect all .cpp files under src
# --------------------------------------------------
file(GLOB_RECURSE ALL_SRC_FILES
    "${SRC_ROOT}/*.cpp"
)

# --------------------------------------------------
# Filter only backend-relevant files
# --------------------------------------------------
set(SERVER_FILES "")
foreach(file ${ALL_SRC_FILES})
    if(file MATCHES "AtlasNet/Server" OR
       file MATCHES "Partition" OR
       file MATCHES "TestUnityAPI/Server")
        if(NOT file MATCHES "Client" AND
           NOT file MATCHES "Editor" AND
           NOT file MATCHES "ImGui" AND
           NOT file MATCHES "Interface" AND
           NOT file MATCHES "Render" AND
           NOT file MATCHES "Test")
            list(APPEND SERVER_FILES ${file})
        endif()
    endif()
endforeach()

if(NOT SERVER_FILES)
    message(FATAL_ERROR "No backend server files found under ${SRC_ROOT}")
endif()

# --------------------------------------------------
# Include directories dynamically
# --------------------------------------------------
include_directories(
    ${SRC_ROOT}
    ${SRC_ROOT}/AtlasNet/Server
    ${SRC_ROOT}/Partition
    ${SRC_ROOT}/TestUnityAPI/Server
)

# --------------------------------------------------
# Build AtlasUnityBridge (shared plugin)
# --------------------------------------------------
add_library(AtlasUnityBridge SHARED ${SERVER_FILES})

set_target_properties(AtlasUnityBridge PROPERTIES
    OUTPUT_NAME "AtlasUnityBridge"
)

# Disable warnings for MSVC
if(MSVC)
    target_compile_definitions(AtlasUnityBridge PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --------------------------------------------------
# Future targets (optional)
# --------------------------------------------------
# Example: Partition executable
# add_executable(PartitionApp Partition/main.cpp)
# target_link_libraries(PartitionApp PRIVATE AtlasUnityBridge)

message(STATUS "Collected ${SERVER_FILES}")
