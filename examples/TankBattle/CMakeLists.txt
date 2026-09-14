cmake_minimum_required(VERSION 3.10)

# Recursively collect sources under ./src
file(GLOB_RECURSE _core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)

add_library(TankBattleCore ${_core_sources})
# Optional, but common: local includes
target_include_directories(TankBattleCore
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)
target_link_libraries(TankBattleCore AtlasNetCore)

# Recursively collect sources under ./src
file(GLOB_RECURSE _shard_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/shard/*.cpp"
)

add_executable(TankBattleShard ${_shard_sources})
# Optional, but common: local includes
target_include_directories(TankBattleShard
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shard"
)

target_link_libraries(TankBattleShard TankBattleCore AtlasNetServerAPI)
file(GLOB_RECURSE _client_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.cpp"
)
add_executable(TankBattleClient ${_client_sources})
target_include_directories(TankBattleClient
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/client"
)
target_link_libraries(TankBattleClient TankBattleCore AtlasNetClientAPI raylib glfw)


add_custom_target(TankBattleStage
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/.stage"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:TankBattleShard>
        "${CMAKE_CURRENT_SOURCE_DIR}/.stage/"

    DEPENDS TankBattleShard
)
add_custom_target(TankBattleDockerDevBuild
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND docker build -t tankbattle-dev:latest .
DEPENDS TankBattleStage
USES_TERMINAL
)

add_custom_target(TankBattleDockerDeploy
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    # If the stack exists, kill it and wait 1 second
    COMMAND docker stack rm tankbattle || true
    COMMAND sleep 1
    COMMAND docker stack deploy -c TankBattleStack.yml tankbattle
    USES_TERMINAL
DEPENDS TankBattleDockerDevBuild AtlasNetDevDockerBuild
)

add_custom_target(TankBattlek8sDismantle
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND kubectl delete -f TankBattle-k8s.yml || true
    USES_TERMINAL
)
# this one can run in parallel
add_custom_target(TankBattleK8sLoadImages
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND minikube image load tankbattle-dev:latest & 
    minikube image load atlasnet-controller-dev:latest & 
    minikube image load atlasnet-gateway-dev:latest & 
    minikube image load atlasnet-cartograph-frontend-dev:latest & 
    minikube image load atlasnet-cartograph-backend-dev:latest
    USES_TERMINAL
DEPENDS TankBattleDockerDevBuild AtlasNetDevDockerBuild TankBattlek8sDismantle
)
add_custom_target(TankBattleK8sDeploy
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    # If the stack exists, kill it and wait 1 second
    COMMAND kubectl apply -f TankBattle-k8s.yml
    DEPENDS TankBattleK8sLoadImages
    USES_TERMINAL
)