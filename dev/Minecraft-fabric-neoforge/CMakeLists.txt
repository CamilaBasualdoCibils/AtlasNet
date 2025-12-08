cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# 1) Gradle build target
add_custom_target(${_target_name} ALL
  COMMAND ${CMAKE_COMMAND} -E env
          GRADLE_USER_HOME=${CMAKE_CURRENT_LIST_DIR}/build/.gradle
          ${CMAKE_CURRENT_LIST_DIR}/src/gradlew --console=auto --info build
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src
  USES_TERMINAL
)

# 2) Ensure Bootstrap is compiled AFTER the Gradle project builds
# (Bootstrap must already be defined somewhere else via add_executable(Bootstrap ...))
add_dependencies(Bootstrap ${_target_name})

# 3) Launch task: builds Gradle + Bootstrap (via deps) then runs Bootstrap
add_custom_target(${_target_name}_launch
  COMMAND ${CMAKE_COMMAND} -E env
          DOCKER_BUILDKIT=1
          ${CMAKE_COMMAND} -E chdir $<TARGET_PROPERTY:Bootstrap,SOURCE_DIR>
          $<TARGET_FILE:Bootstrap>
  DEPENDS Bootstrap
  USES_TERMINAL
)


# ---- AtlasNetServer -> local staging dir ----

# Where you want the output copied to (local to this subproject)
# For multi-config generators (VS/Xcode/Ninja Multi-Config), keep per-config outputs:
set(_atlasnetserver_stage_dir "${CMAKE_CURRENT_LIST_DIR}/staged/AtlasNetServer/$<CONFIG>")

add_custom_target(${_target_name}_stage_atlasnetserver
  DEPENDS AtlasNetServer_Shared
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_atlasnetserver_stage_dir}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:AtlasNetServer_Shared>"
          "${_atlasnetserver_stage_dir}/"
  USES_TERMINAL
)

# If you want Bootstrap to only build after the server is staged too:
add_dependencies(Bootstrap ${_target_name}_stage_atlasnetserver)

# Or, if you only want it for launching:
# add_dependencies(${_target_name}_launch ${_target_name}_stage_atlasnetserver)
