cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# ------------------------------------------------------------------------------
# AtlasNetServer (C++ -> Java dist) -> stage into this Gradle project
#   Producer target (from CMake1): AtlasNetServer_Java
#   Exposed properties on that target:
#     ATLASNETSERVER_JAVA_DIST_DIR
#     ATLASNETSERVER_JAVA_DIST_JAR
#     ATLASNETSERVER_JAVA_DIST_NATIVES_DIR (optional, not needed here)
# ------------------------------------------------------------------------------

# Where gradlew lives
# Where gradlew lives
# Where gradlew lives
set(_gradle_root "${CMAKE_CURRENT_LIST_DIR}/src")

# Generated/ignored staging area inside the Gradle root
set(_stage_root "${_gradle_root}/.atlasnet")
set(_stage_nat  "${CMAKE_CURRENT_LIST_DIR}/natives")

# Producer outputs (full paths)
set(_dist_dir "$<TARGET_PROPERTY:AtlasNetServer_Java,ATLASNETSERVER_JAVA_DIST_DIR>")
set(_dist_jar "$<TARGET_PROPERTY:AtlasNetServer_Java,ATLASNETSERVER_JAVA_DIST_JAR>")

add_custom_target(${_target_name}_stage_atlasnetserver_java
  DEPENDS AtlasNetServer_Java
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_root}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_nat}"

  # Copy jar into the folder, preserving whatever filename it has
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_dist_jar}"
          "${_stage_root}"

  # Copy natives
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${_dist_dir}/natives"
          "${_stage_nat}"

  USES_TERMINAL
)


# ------------------------------------------------------------------------------
# Gradle build target
# ------------------------------------------------------------------------------

add_custom_target(${_target_name} ALL
  COMMAND ${CMAKE_COMMAND} -E env
          GRADLE_USER_HOME=${CMAKE_CURRENT_LIST_DIR}/build/.gradle
          ${CMAKE_CURRENT_LIST_DIR}/src/gradlew --console=auto --info build
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src
  USES_TERMINAL
)

# Ensure Gradle build runs AFTER staging (and thus after AtlasNetServer_Java produced dist)
add_dependencies(${_target_name} ${_target_name}_stage_atlasnetserver_java)

# ------------------------------------------------------------------------------
# Ensure Bootstrap is compiled AFTER the Gradle project builds
# (Bootstrap must already be defined somewhere else via add_executable(Bootstrap ...))
# ------------------------------------------------------------------------------

add_dependencies(Bootstrap ${_target_name})

# ------------------------------------------------------------------------------
# Launch task: builds Gradle + Bootstrap (via deps) then runs Bootstrap
# ------------------------------------------------------------------------------

add_custom_target(${_target_name}_launch
  COMMAND ${CMAKE_COMMAND} -E env
          DOCKER_BUILDKIT=1
          ${CMAKE_COMMAND} -E chdir $<TARGET_PROPERTY:Bootstrap,SOURCE_DIR>
          $<TARGET_FILE:Bootstrap>
  DEPENDS Bootstrap
  USES_TERMINAL
)
