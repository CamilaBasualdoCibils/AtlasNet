#pragma once
#include "AtlasNet/Core/Core.hpp"
#include "boost/graph/properties.hpp"
#include <boost/graph/adjacency_list.hpp>
namespace AtlasNet
{
namespace Network
{


enum class PollType
{
  Blocking,
  NonBlocking
};

} // namespace Network
} // namespace AtlasNet