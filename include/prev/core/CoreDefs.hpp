#pragma once
#include <nlohmann/json.hpp>
#include "atlasnet/core/utils/Snowflake.hpp"
#include "atlasnet/core/utils/StrongTypedef.hpp"
namespace AtlasNet
{

using _Json = nlohmann::json;
using _JsonOrdered = nlohmann::ordered_json;

using AtlasNetNodeID = StrongTypedef<uint32_t, struct NodeIDTag>;
using AtlasNetShardID = StrongTypedef<uint32_t, struct ShardIDTag>;
using AtlasNetGatewayID = StrongTypedef<uint32_t, struct GatewayIDTag>;
using AtlasNetControllerID = StrongTypedef<uint32_t, struct ControllerIDTag>;

/**
 * @brief Represents a unique identifier for an entity within the AtlasNet system.
 * 4-bit sequence, 7-bit worker, and 53-bit timestamp.
 * 
 */
using AtlasNetEntityID = Snowflake;

using AtlasNetClientID = Snowflake;
} // namespace AtlasNet