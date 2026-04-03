

# SERVER


add_executable(MessageTestsRemoteServer ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp)
# Optional, but common: local includes

target_link_libraries(MessageTestsRemoteServer
AtlasNetCore
AtlasNetTestCrashUtils
)
target_compile_definitions(MessageTestsRemoteServer PRIVATE)

# CLIENT
add_executable(MessageTestsRemoteClient ${CMAKE_CURRENT_SOURCE_DIR}/Client.cpp)
# Optional, but common: local includes
target_link_libraries(MessageTestsRemoteClient
AtlasNetCore
AtlasNetTestCrashUtils
)

target_compile_definitions(MessageTestsRemoteClient PRIVATE )
# Tests

#add_executable(MessageTestsRemote ${CMAKE_CURRENT_SOURCE_DIR}/Tests.cpp)
## Optional, but common: local includes
#
#target_link_libraries(MessageTestsRemote
#AtlasNetTestCrashUtils
#gtest
#)

#target_compile_definitions(MessageTestsRemote PRIVATE 
#SERVER_BINARY="$<TARGET_FILE:MessageTestsRemoteServer>"
#CLIENT_BINARY="$<TARGET_FILE:MessageTestsRemoteClient>"
#)

#add_dependencies(MessageTestsRemote MessageTestsRemoteServer MessageTestsRemoteClient)
#
#gtest_discover_tests(MessageTestsRemote)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
  NAME MessageTest0
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_0
)

add_test(
  NAME MessageTest1
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_1
)

add_test(
  NAME MessageTest2
  COMMAND ${VENV_PYTHON} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/MessageTests.py -k test_message_2
)
set_tests_properties(MessageTest0 MessageTest1 MessageTest2 PROPERTIES
  ENVIRONMENT
    "SERVER_BINARY=$<TARGET_FILE:MessageTestsRemoteServer>;CLIENT_BINARY=$<TARGET_FILE:MessageTestsRemoteClient>"
)