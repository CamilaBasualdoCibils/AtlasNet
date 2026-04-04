file(GLOB_RECURSE _Scene_View_Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(AtlasNetTestSceneView STATIC ${_Scene_View_Sources})
target_include_directories(AtlasNetTestSceneView PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AtlasNetTestSceneView PUBLIC AtlasNetCore raylib)