#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <span>
#include <bit>
#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/packing.hpp>

struct ByteError : std::runtime_error {
  using std::runtime_error::runtime_error;
};