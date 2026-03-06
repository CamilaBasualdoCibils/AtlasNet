cmake_minimum_required(VERSION 3.6)

option(SANDBOX_ADD_ATLASNET        "Include AtlasNet" ON)
if ((NOT ATLASNET_ROOT) AND SANDBOX_ADD_ATLASNET)
add_subdirectory(sandbox/.AtlasNet)
endif()

set(SANDBOX_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Root directory of Sandbox")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SANDBOX_ADD_CLIENT        "Include AtlasNet" OFF)

add_subdirectory(lib)
add_subdirectory(server)
if(SANDBOX_ADD_CLIENT)
add_subdirectory(client)
endif()



add_custom_target(sandbox_atlasnet_run
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dDev.sh atlasnet-dev ${SANDBOX_SERVER_IMAGE_NAME}
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
     USES_TERMINAL
     DEPENDS AtlasnetDockerBuild_Fast SandboxServerDockerBuild
     )

add_custom_target(sandbox_atlasnet_run_fast
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dDev.sh atlasnet-dev ${SANDBOX_SERVER_IMAGE_NAME}
     USES_TERMINAL
     DEPENDS AtlasnetDockerBuild_FastCached SandboxServerDockerBuild_Fast
     )

add_custom_target(sandbox_atlasnet_run_debug
     COMMAND ${CMAKE_COMMAND} -E env
             ATLASNET_HOTSPOT_CMAKE_GENERATOR=${CMAKE_GENERATOR}
             ATLASNET_HOTSPOT_CMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
             ATLASNET_HOTSPOT_CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
             ATLASNET_HOTSPOT_VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}
             ATLASNET_HOTSPOT_VCPKG_HOST_TRIPLET=${VCPKG_HOST_TRIPLET}
             ATLASNET_HOTSPOT_VCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}
             ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dHotspot.sh
             ${CMAKE_SOURCE_DIR}
             ${CMAKE_SOURCE_DIR}/build-hotspot
     USES_TERMINAL
     COMMENT "Configures and runs AtlasNet on k3d with Hotspot-friendly build flags"
     )

set(ATLASNET_HOTSPOT_CLUSTER_NAME "atlasnet-dev" CACHE STRING "k3d cluster name used by hotspot profiling target")
set(ATLASNET_HOTSPOT_DURATION_S "30" CACHE STRING "Profiling duration in seconds for hotspot profiling target")
set(ATLASNET_HOTSPOT_OPEN_MODE "novnc" CACHE STRING "Hotspot auto-open mode: container|host|novnc|none")
set(ATLASNET_HOTSPOT_AUTO_OPEN "1" CACHE STRING "Auto-open captures after record: 1=yes, 0=no")
set(ATLASNET_HOTSPOT_NOVNC_PORT_BASE "16080" CACHE STRING "Base host port for noVNC viewers (one per perf file)")
set(ATLASNET_HOTSPOT_NOVNC_AUTO_OPEN_BROWSER "1" CACHE STRING "Auto-open noVNC URLs in browser: 1=yes, 0=no")

add_custom_target(sandbox_atlasnet_run_debug_profile
     COMMAND ${CMAKE_COMMAND} -E env
             ATLASNET_HOTSPOT_DURATION_S=${ATLASNET_HOTSPOT_DURATION_S}
             ATLASNET_HOTSPOT_OPEN_MODE=${ATLASNET_HOTSPOT_OPEN_MODE}
             ATLASNET_HOTSPOT_AUTO_OPEN=${ATLASNET_HOTSPOT_AUTO_OPEN}
             ATLASNET_HOTSPOT_NOVNC_PORT_BASE=${ATLASNET_HOTSPOT_NOVNC_PORT_BASE}
             ATLASNET_HOTSPOT_NOVNC_AUTO_OPEN_BROWSER=${ATLASNET_HOTSPOT_NOVNC_AUTO_OPEN_BROWSER}
             ${CMAKE_SOURCE_DIR}/Dev/ProfileK3dHotspot.sh
             ${ATLASNET_HOTSPOT_CLUSTER_NAME}
     USES_TERMINAL
     COMMENT "Profiles the existing k3d cluster and opens Hotspot captures (no cluster recreate/deploy)"
     )
