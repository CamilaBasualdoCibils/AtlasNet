#pragma once
#include "AtlasNet/Core/Types/Snowflake.hpp"
#include "AtlasNet/Core/Types/StrongTypedef.hpp"
#include "AtlasNet/Core/Types/UUID.hpp"
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
namespace AtlasNet
{

/* using _Json = nlohmann::json;
using _JsonOrdered = nlohmann::ordered_json;
 */
using AtlasNetNodeID = UUID;
using AtlasNetDBID = UUID;
/* using AtlasNetShardID = StrongTypedef<uint32_t, struct ShardIDTag>;
using AtlasNetGatewayID = StrongTypedef<uint32_t, struct GatewayIDTag>;
using AtlasNetControllerID = StrongTypedef<uint32_t, struct ControllerIDTag>; */

/**
 * @brief Represents a unique identifier for an entity within the AtlasNet
 * system. 4-bit sequence, 7-bit worker, and 53-bit timestamp.
 *
 */
using AtlasNetEntityID = Snowflake;

using AtlasNetClientID = Snowflake;

using _Json = nlohmann::json;
} // namespace AtlasNet



