#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Memory/Memory.hpp"
#include "boost/graph/properties.hpp"
#include <boost/graph/adjacency_list.hpp>
#include <cstddef>
#include <vector>

namespace AtlasNet::Network
{
using ByteBuffer =
    std::vector<std::byte, AtlasNet::Memory::Allocator<std::byte>>;

enum class PollType
{
  Blocking,
  NonBlocking
};
} // namespace AtlasNet::Network
