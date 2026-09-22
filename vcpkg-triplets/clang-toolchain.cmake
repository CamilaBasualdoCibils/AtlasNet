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
