# clang-toolchain.cmake
# Prefer a recent clang for C++23 (std::expected, etc.). Fall back to default.

if(EXISTS "/usr/bin/clang++-18")
  set(CMAKE_C_COMPILER /usr/bin/clang-18)
  set(CMAKE_CXX_COMPILER /usr/bin/clang++-18)
elseif(EXISTS "/usr/bin/clang++-19")
  set(CMAKE_C_COMPILER /usr/bin/clang-19)
  set(CMAKE_CXX_COMPILER /usr/bin/clang++-19)
else()
  set(CMAKE_C_COMPILER /usr/bin/clang)
  set(CMAKE_CXX_COMPILER /usr/bin/clang++)
endif()

# Use libc++ explicitly so C++23 library facilities such as std::expected are
# available consistently on CI and local Clang installations.
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++")
