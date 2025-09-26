cmake_minimum_required(VERSION 3.16)
project(KDNetUnityAPI LANGUAGES CXX)

add_library(KDNetUnityAPI SHARED KDNetUnityAPI.cpp)
target_compile_features(KDNetUnityAPI PUBLIC cxx_std_17)

# Remove 'lib' prefix on Windows so the DLL is KDNetUnityAPI.dll
if(WIN32)
  set_target_properties(KDNetUnityAPI PROPERTIES PREFIX "")
  target_link_libraries(KDNetUnityAPI PRIVATE winhttp)   # <- Windows system lib
else()
  find_package(CURL REQUIRED)
  target_link_libraries(KDNetUnityAPI PRIVATE CURL::libcurl)
endif()
