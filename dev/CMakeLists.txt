

set(DEV_STAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.stage")

add_custom_target(AtlasNetDevDockerBuildStage
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEV_STAGE_DIR}"


    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:AtlasNetNode>
        "${DEV_STAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:AtlasNetDB>
        "${DEV_STAGE_DIR}/"

    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:AtlasNetCartographBackend>
        "${DEV_STAGE_DIR}/"

    DEPENDS
        $<TARGET_FILE:AtlasNetCartographBackend>
        $<TARGET_FILE:AtlasNetNode>
        $<TARGET_FILE:AtlasNetDB>

    USES_TERMINAL
    VERBATIM
)
set_target_properties(AtlasNetDevDockerBuildStage PROPERTIES FOLDER "AtlasNet/Docker")
add_custom_target(AtlasNetDevDockerBuild
    COMMAND ./dev/BuildDevImages.sh
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS AtlasNetDevDockerBuildStage
    USES_TERMINAL
    VERBATIM
)
set_target_properties(AtlasNetDevDockerBuild PROPERTIES FOLDER "AtlasNet/Docker")
add_custom_target(AtlasNetDevDockerBuild_Valgrind
    COMMAND ./dev/BuildDevImages.sh "valgrind {EXE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS AtlasNetDevDockerBuildStage
    USES_TERMINAL
    VERBATIM
)
set_target_properties(AtlasNetDevDockerBuild_Valgrind PROPERTIES FOLDER "AtlasNet/Docker")


if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(DOCKER_ARCH amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(DOCKER_ARCH arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
    set(DOCKER_ARCH arm/v7)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
    set(DOCKER_ARCH 386)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_custom_target(AtlasNetProductionDockerBuild
    COMMAND ./dev/BuildProductionImages.sh ${DOCKER_ARCH}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM)
set_target_properties(AtlasNetProductionDockerBuild PROPERTIES FOLDER "AtlasNet/Docker")



add_custom_target(AtlasNetDockerDeploy_Dev
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    # If the stack exists, kill it and wait 1 second
    COMMAND docker stack rm atlasnet || true
    COMMAND sleep 1
    COMMAND docker stack deploy -c DevDocker.yml atlasnet
    USES_TERMINAL
DEPENDS AtlasNetDevDockerBuild AtlasNetDevDockerBuild
)
set_target_properties(AtlasNetDockerDeploy_Dev PROPERTIES FOLDER "AtlasNet/Docker")
