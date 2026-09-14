include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

#
#FetchContent_Declare(
#    docker-cpp
#    GIT_REPOSITORY https://github.com/perkss/docker-cpp.git
#    GIT_TAG        main
#)
#FetchContent_MakeAvailable(docker-cpp)

if (NOT TARGET gtest)
FetchContent_Declare(
  gtest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(gtest)
endif()

if (NOT TARGET raylib)
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)
endif()




set(VENV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.venv" CACHE PATH "Path to Python virtual environment for tests")
set(VENV_PIP "${VENV_DIR}/bin/pip" CACHE FILEPATH "Path to pip executable in the Python virtual environment")
set(VENV_PYTHON "${VENV_DIR}/bin/python" CACHE FILEPATH "Path to Python executable in the Python virtual environment")
# Step 1: Create venv
if(EXISTS "${VENV_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${VENV_DIR}"
        RESULT_VARIABLE RM_VENV_RESULT
    )
    if(NOT RM_VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to remove existing Python venv folder: ${VENV_DIR}")
    endif()
endif()
execute_process(
    COMMAND python3 -m venv ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE VENV_RESULT
)
if(NOT VENV_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to create Python venv")
endif()

# Step 2: Install using the venv's pip directly (no activation needed)
execute_process(
    COMMAND ${VENV_PIP} install -r ${CMAKE_CURRENT_SOURCE_DIR}/PythonRequirements.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE PIP_RESULT
)
if(NOT PIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to install Python dependencies " ${PIP_RESULT})
endif()