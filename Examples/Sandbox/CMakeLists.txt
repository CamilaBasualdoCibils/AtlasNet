cmake_minimum_required(VERSION 3.16)
project(Sandbox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ATLASNET_TARGET "${CMAKE_CURRENT_SOURCE_DIR}/.AtlasNet")
execute_process(
    COMMAND "/workspaces/AtlasNetNew/AtlasNet/AtlasNetInstall.sh" ${ATLASNET_TARGET}   # remove if already exists
)


include(.AtlasNet/AtlasNetSDK.cmake)
add_subdirectory(lib)
add_subdirectory(client)
add_subdirectory(server)

add_custom_target(run_sandbox
    COMMAND  ${ATLASNET_ROOT}/bin/AtlasNet 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

