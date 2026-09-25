# gcc-toolchain.cmake

if(EXISTS "/usr/bin/g++-14")
  set(CMAKE_C_COMPILER /usr/bin/gcc-14)
  set(CMAKE_CXX_COMPILER /usr/bin/g++-14)
else()
  set(CMAKE_C_COMPILER /usr/bin/gcc)
  set(CMAKE_CXX_COMPILER /usr/bin/g++)
endif()
