cmake_minimum_required(VERSION 3.24)
project(SampleSetup)
set(CMAKE_TOOLCHAIN_FILE "AtlasNet/AtlasNetToolchain.cmake" CACHE FILEPATH "AtlasNet toolchain")





add_subdirectory(samples/Sandbox/server)