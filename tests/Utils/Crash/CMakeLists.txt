
file(GLOB_RECURSE _Crash_Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(AtlasNetTestCrashUtils STATIC ${_Crash_Sources})
target_include_directories(AtlasNetTestCrashUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AtlasNetTestCrashUtils PUBLIC AtlasNetCore)



