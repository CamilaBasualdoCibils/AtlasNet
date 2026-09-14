#include "atlasnet/core/node/NodeRegistry.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"

std::optional<AtlasNet::NodeInfo>
AtlasNet::NodeRegistry::RegisterNode(const AtlasNetNodeType type,
                                     const Network::SocketAddress& address)
{
  NodeInfo info;
  info.containerType = type;
  info.address = address;
  std::optional<AtlasNetNodeID> nodeID = ClaimNodeID(address);
  if (!nodeID.has_value())
  {
    logger->error("Failed to claim Node ID for address {}",
                  address.to_string());
    return std::nullopt;
  }
  info.id = *nodeID;

  if (type == AtlasNetNodeType::Controller)
  {
    ControllerNodeInfo controllerInfo;
    const std::optional<AtlasNetControllerID> controllerID =
        ClaimControllerID(info.id);
    if (!controllerID.has_value())
    {
      logger->error("Failed to claim Controller ID for node ID {}",
                    info.id.value);
      return std::nullopt;
    }
    controllerInfo.id = *controllerID;
    info.specificInfo = controllerInfo;
    // Handle Controller node registration
  }
  else if (type == AtlasNetNodeType::Shard)
  {
    ShardNodeInfo shardInfo;
    const std::optional<AtlasNetShardID> shardID = ClaimShardID(info.id);
    if (!shardID.has_value())
    {
      logger->error("Failed to claim Shard ID for node ID {}", info.id.value);
      return std::nullopt;
    }
    shardInfo.id = *shardID;
    info.specificInfo = shardInfo;
    // Handle Shard node registration
  }
  else if (type == AtlasNetNodeType::Gateway)
  {
    GatewayNodeInfo gatewayInfo;
    const std::optional<AtlasNetGatewayID> gatewayID = ClaimGatewayID(info.id);
    if (!gatewayID.has_value())
    {
      logger->error("Failed to claim Gateway ID for node ID {}", info.id.value);
      return std::nullopt;
    }
    gatewayInfo.id = *gatewayID;
    info.specificInfo = gatewayInfo;
    // Handle Gateway node registration
  }
  else if (type == AtlasNetNodeType::Cartograph)
  {
    info.specificInfo = CartographBackendInfo{};
    // Handle CartographBackend node registration
  }
  else
  {
    throw std::runtime_error("Unknown node type");
  }
  return info;
}
std::optional<AtlasNet::AtlasNetNodeID>
AtlasNet::NodeRegistry::ClaimNodeID(const Network::SocketAddress& address)
{
  if (NodeIdLease.has_value())
  {
    throw std::runtime_error("Node ID lease already exists");
  }

  logger->info("Claiming Node ID for address {} at table {} for max ID {}",
               address.to_string(), NodeIDLeaseTable,
               std::numeric_limits<AtlasNetNodeID::underlying_type_t>::max());
  std::optional<uint64_t> result = ClaimIDHashTable(
      _redisConn, NodeIDLeaseTable, address.to_string(),
      std::numeric_limits<AtlasNetNodeID::underlying_type_t>::max());
  if (!result.has_value())
  {
    return std::nullopt;
  }
  NodeIdLease.emplace(*this, NodeIDLeaseTable,
                      std::to_string(result.value()));
  NodeIdLease->Init();
  return static_cast<AtlasNetNodeID>(*result);
}
std::optional<AtlasNet::AtlasNetShardID>
AtlasNet::NodeRegistry::ClaimShardID(AtlasNetNodeID nodeID)
{
  if (ShardIdLease.has_value())
  {
    throw std::runtime_error("Shard ID lease already exists");
  }

  logger->info("Claiming Shard ID for node ID {} at table {} for max ID {}",
               nodeID.value, ShardIDLeaseTable,
               std::numeric_limits<AtlasNetShardID::underlying_type_t>::max());
  std::optional<uint64_t> result = ClaimIDHashTable(
      _redisConn, ShardIDLeaseTable, std::to_string(nodeID.value),
      std::numeric_limits<AtlasNetShardID::underlying_type_t>::max());
  if (!result.has_value())
  {
    return std::nullopt;
  }
  ShardIdLease.emplace(*this, ShardIDLeaseTable,
                       std::to_string(result.value()));
  ShardIdLease->Init();
  return static_cast<AtlasNetShardID>(*result);
}
std::optional<AtlasNet::AtlasNetGatewayID>
AtlasNet::NodeRegistry::ClaimGatewayID(AtlasNetNodeID nodeID)
{
  if (GatewayIdLease.has_value())
  {
    throw std::runtime_error("Gateway ID lease already exists");
  }

  logger->info(
      "Claiming Gateway ID for node ID {} at table {} for max ID {}",
      nodeID.value, GatewayIDLeaseTable,
      std::numeric_limits<AtlasNetGatewayID::underlying_type_t>::max());
  std::optional<uint64_t> result = ClaimIDHashTable(
      _redisConn, GatewayIDLeaseTable, std::to_string(nodeID.value),
      std::numeric_limits<AtlasNetGatewayID::underlying_type_t>::max());
  if (!result.has_value())
  {
    return std::nullopt;
  }
  GatewayIdLease.emplace(*this, GatewayIDLeaseTable,
                         std::to_string(result.value()));
  GatewayIdLease->Init();
  return static_cast<AtlasNetGatewayID>(*result);
}
std::optional<AtlasNet::AtlasNetControllerID>
AtlasNet::NodeRegistry::ClaimControllerID(AtlasNetNodeID nodeID)
{
  if (ControllerIdLease.has_value())
  {
    throw std::runtime_error("Controller ID lease already exists");
  }

  logger->info(
      "Claiming Controller ID for node ID {} at table {} for max ID {}",
      nodeID.value, ControllerIDLeaseTable,
      std::numeric_limits<AtlasNetControllerID::underlying_type_t>::max());
  std::optional<uint64_t> result = ClaimIDHashTable(
      _redisConn, ControllerIDLeaseTable, std::to_string(nodeID.value),
      std::numeric_limits<AtlasNetControllerID::underlying_type_t>::max());
  if (!result.has_value())
  {
    return std::nullopt;
  }
  ControllerIdLease.emplace(*this, ControllerIDLeaseTable,
                            std::to_string(result.value()));
  ControllerIdLease->Init();
  return static_cast<AtlasNetControllerID>(*result);
};