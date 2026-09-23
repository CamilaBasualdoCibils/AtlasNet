#pragma once
#include "AtlasNet/Core/Types/StrongTypedef.hpp"
#include <cstdint>
namespace AtlasNet
{
using ShardID = StrongTypedef<uint32_t, struct ShardIDTag>;
using RegionID = StrongTypedef<uint64_t, struct RegionIDTag>;
}
