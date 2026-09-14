#pragma once
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/database/redis/Redis.hpp"
#include "atlasnet/core/database/redis/RedisConn.hpp"
#include "atlasnet/core/database/redis/utils/RedisUtils.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "enviroment/Enviroment.hpp"
#include "spdlog/logger.h"
#include "sw/redis++/redis.h"
#include <cassert>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
namespace AtlasNet
{

class NodeRegistry
{
  constexpr static int ID_LEASE_TTL_SECONDS = 30,
                       ID_LEASE_RENEW_INTERVAL_SECONDS = 10;
  class IDLease
  {
  public:
    IDLease(NodeRegistry& registry) : _registry(registry) {}
    void Init()
    {
      TTLThread = std::jthread(
          [this]()
          {
            while (true)
            {
              std::this_thread::sleep_for(
                  std::chrono::seconds(ID_LEASE_RENEW_INTERVAL_SECONDS));

              RenewIDLease();
            }
          });
    }

  protected:
    virtual void RenewIDLease() = 0;
    NodeRegistry& _registry;
    std::jthread TTLThread;
  };
  class IDLeaseHashMap : public IDLease
  {
  public:
    IDLeaseHashMap(NodeRegistry& registry, const std::string_view& hashName,
                   const std::string_view& ID)
        : IDLease(registry), _Hashname(hashName), _ID(ID)
    {
    }

  private:
    void RenewIDLease() override
    {
      _registry._redisConn->HashMap().TTL().HExpire(_Hashname, _ID,
                                                    ID_LEASE_TTL_SECONDS);
    }
    const std::string _Hashname;
    const std::string _ID;
  };

public:
  struct Config
  {
    Database::RedisConn* redisConn;
  };
  NodeRegistry(const Config& config)
  {
    _redisConn = config.redisConn;
    assert(_redisConn != nullptr);
  }

  /*
    struct ServiceInfo
    {

      HostAddress address;
      AtlasNetNodeType containerType;

      void Serialize(ByteWriter& archive) const
      {
        archive(id);
        archive(address);
        archive(containerType);
        // archive(overlayAddress);
      }

      void Deserialize(ByteReader& archive)
      {
        archive(id);
        archive(address);
        archive(containerType);
        // archive(overlayAddress);

      }

      void to_json(_Json& j) const
      {
        j = _Json{
            {"id", id.to_string()},
            {"address", address.to_string()},
            {"containerType",
             boost::describe::enum_to_string(containerType, "UNKNOWN")},
            //{"overlayAddress", overlayAddress.to_string()

        };
      }
    };
    void RegisterService(ServiceInfo info)
    {

      ByteWriter bwKey;
      bwKey(info.id);
      ByteWriter bw;
      info.Serialize(bw);
      _redisConn->HashMap().GetSet().HSet(ContainerID2ServiceInfoKey,
                                          bwKey.as_string_view(),
                                          bw.as_string_view());

      _redisConn->Set().Modify().SAdd(
          GetContainerType2ContainerIDsSetKey(info.containerType),
          bwKey.as_string_view());
      if (Env::DebugMode)
      {
        _Json debugJson;
        info.to_json(debugJson);
        _redisConn->HashMap().GetSet().HSet(ContainerID2ServiceInfoKey +
    "_debug", std::to_string(info.id), debugJson.dump(4));
        _redisConn->Set().Modify().SAdd(
            GetContainerType2ContainerIDsSetKey(info.containerType) + "_debug",
            std::to_string(info.id));
      }
    }

    void GetServicesOfType(AtlasNetNodeType type,
                           std::vector<ServiceInfo>& outServices)
    {
      std::unordered_set<AtlasNetNodeID> containerIDs;
      GetContainerIDsOfType(type, containerIDs);
      for (const auto& containerID : containerIDs)
      {
        ByteWriter bwKey;
        bwKey(containerID);

        if (std::optional<std::string> serviceInfoStr =
                _redisConn->HashMap().GetSet().HGet(ContainerID2ServiceInfoKey,
                                                    bwKey.as_string_view());
            serviceInfoStr.has_value())
        {
          ByteReader br(*serviceInfoStr);
          ServiceInfo info;
          info.Deserialize(br);
          outServices.push_back(std::move(info));
        }
      }
    }

    void GetContainerIDsOfType(AtlasNetNodeType type,
                               std::unordered_set<AtlasNetNodeID>& outNodeIDs)
    {
      std::unordered_set<std::string> containerIDStrs;
      _redisConn->Set().Query().SMembers(
          GetContainerType2ContainerIDsSetKey(type),
          std::inserter(containerIDStrs, containerIDStrs.end()));
      for (const auto& containerIDStr : containerIDStrs)
      {
        ByteReader br(containerIDStr);
        AtlasNetNodeID nodeID;
        br(nodeID);
        outNodeIDs.insert(nodeID);
      }
    }

    std::optional<ServiceInfo> GetServiceInfo(const AtlasNetNodeID& id)
    {
      ByteWriter bwKey;
      bwKey(id);
      if (std::optional<std::string> serviceInfoStr =
              _redisConn->HashMap().GetSet().HGet(ContainerID2ServiceInfoKey,
                                                  bwKey.as_string_view());
          serviceInfoStr.has_value())
      {
        ByteReader br(*serviceInfoStr);
        ServiceInfo info;
        info.Deserialize(br);
        return info;
      }
      return std::nullopt;
    } */

  std::optional<NodeInfo> RegisterNode(const AtlasNetNodeType type,
                                       const Network::SocketAddress& address);

  template <typename OutputIt> uint64_t GetAllNodeIDs(OutputIt out) const;

  template <std::output_iterator<AtlasNetShardID> OutputIt>
  uint64_t GetAllShardIDs(OutputIt out) const;

  template <typename IdType>
  std::optional<Network::SocketAddress> ResolveAddress(const IdType& id);
  template <typename IdType>
  std::optional<AtlasNetNodeID> GetNodeID(const IdType& id);

private:
  std::optional<AtlasNetNodeID> ClaimNodeID(const Network::SocketAddress& address);
  std::optional<AtlasNetShardID> ClaimShardID(AtlasNetNodeID nodeID);
  std::optional<AtlasNetGatewayID> ClaimGatewayID(AtlasNetNodeID nodeID);
  std::optional<AtlasNetControllerID> ClaimControllerID(AtlasNetNodeID nodeID);

  template <typename Func>
  static void for_each_keyval_hashtable(Database::RedisConn* r,
                                        std::string_view TableName, Func f)
    requires std::is_invocable_v<Func, std::string_view, std::string_view>
  {
    boost::container::small_vector<std::string, 64> RawshardIDs;

    r->HashMap().GetSet().HGetAll(TableName, std::back_inserter(RawshardIDs));

    for (uint64_t i = 0; i < RawshardIDs.size() / 2; ++i)
    {
      const auto& rawShardID =
          RawshardIDs[i * 2]; // Get the key part, skipping the value
      const auto& rawNodeID =
          RawshardIDs[i * 2 +
                      1]; // Get the value part, corresponding to the key
      f(rawShardID, rawNodeID);
    }
  }

  template <typename Func>
  static void for_each_key_hashtable(Database::RedisConn* r,
                                     std::string_view TableName, Func f)
    requires std::is_invocable_v<Func, std::string_view>
  {
    boost::container::small_vector<std::string, 64> RawKeys;

    r->HashMap().GetSet().HKeys(TableName, std::back_inserter(RawKeys));

    for (uint64_t i = 0; i < RawKeys.size(); ++i)
    {
      const auto& rawKey = RawKeys[i]; // Get the key part, skipping the value
      f(rawKey);
    }
  }

  const static inline std::string NodeRegistryNamespace =
      Env::DatabaseNamespace + "NodeRegistry{node_registry}:";
  const static inline std::string NodeIDLeaseTable = NodeRegistryNamespace +
                                                     "NodeIDLeaseTable",
                                  ShardIDLeaseTable = NodeRegistryNamespace +
                                                      "ShardIDLeaseTable",
                                  GatewayIDLeaseTable = NodeRegistryNamespace +
                                                        "GatewayIDLeaseTable",
                                  ControllerIDLeaseTable =
                                      NodeRegistryNamespace +
                                      "ControllerIDLeaseTable";

  /* std::string GetContainerType2ContainerIDsSetKey(AtlasNetNodeType type)
  {
    return ServiceInfoKeyPrefix +
           std::format(ContainerType2ContainerIDsSetKeyPostFix,
                       boost::describe::enum_to_string(type, "UNKNOWN"));
  } */
  /*  const std::string ServiceInfoKeyPrefix =
       Env::DatabaseNamespace + "ServiceRegistry:";
   const std::string ContainerID2ServiceInfoKey =
       ServiceInfoKeyPrefix + "ContainerID->ServiceInfo";
   constexpr static inline const char* ContainerType2ContainerIDsSetKeyPostFix =
       "{}->ContainerIDs"; */

  Database::RedisConn* _redisConn;
  std::optional<IDLeaseHashMap> NodeIdLease, ShardIdLease, GatewayIdLease,
      ControllerIdLease;
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("NodeRegistry");
};
template <typename IdType>
inline std::optional<Network::SocketAddress>
NodeRegistry::ResolveAddress(const IdType& id)
{
  // TODO: This can be optimized by a lua script to reduce the number of
  // round-trip queries to Redis
  AtlasNetNodeID nodeID;
  if constexpr (!std::is_same_v<IdType, AtlasNetNodeID>)
  {
    if (const std::optional<AtlasNetNodeID> _idRet = GetNodeID(id))
    {
      nodeID = *_idRet;
    }
    else
    {
      return std::nullopt;
    }
  }
  else
  {
    nodeID = id;
  }

  const std::optional<std::string> res =
      _redisConn->HashMap().GetSet().HGet(NodeIDLeaseTable, nodeID.to_string());
  if (res.has_value())
  {
    Network::SocketAddress address;
    address.parse_string(*res);
    return address;
  }
  return std::nullopt;
}

template <typename IdType>
inline std::optional<AtlasNetNodeID> NodeRegistry::GetNodeID(const IdType& id)
{
  std::optional<std::string> fetchResponse;
  if constexpr (std::is_same_v<IdType, AtlasNetNodeID>)
  {
    return id;
  }
  else if constexpr (std::is_same_v<IdType, AtlasNetShardID>)
  {
    fetchResponse =
        _redisConn->HashMap().GetSet().HGet(ShardIDLeaseTable, id.to_string());
  }
  else if constexpr (std::is_same_v<IdType, AtlasNetGatewayID>)
  {
    fetchResponse = _redisConn->HashMap().GetSet().HGet(GatewayIDLeaseTable,
                                                        id.to_string());
  }
  else if constexpr (std::is_same_v<IdType, AtlasNetControllerID>)
  {
    static_assert(sizeof(IdType) == 0, "NOT IMPLEMENTED");
    return std::nullopt;
  }
  else
  {
    static_assert(sizeof(IdType) == 0, "Unsupported IdType");
  }
  if (fetchResponse.has_value())
  {
    return AtlasNetNodeID::from_string(*fetchResponse);
  }
  return std::nullopt;
}

template <std::output_iterator<AtlasNetShardID> OutputIt>
inline uint64_t NodeRegistry::GetAllShardIDs(OutputIt out) const
{
  uint64_t count = 0;
  logger->info("Fetching all shard IDs from the registry");
  for_each_key_hashtable(_redisConn, ShardIDLeaseTable,
                         [&](const auto& rawID)
                         {
                           logger->info("fetched {}", rawID);
                           AtlasNetShardID shardID =
                               AtlasNetShardID::from_string(rawID);
                           *out++ = shardID;
                           count++;
                         });
  return count; // Return the number of shard IDs written
}

template <typename OutputIt>
inline uint64_t NodeRegistry::GetAllNodeIDs(OutputIt out) const
{
  uint64_t count = 0;
  for_each_key_hashtable(_redisConn, NodeIDLeaseTable,
                         [&](const auto& rawID)
                         {
                           AtlasNetNodeID nodeID =
                               AtlasNetNodeID::from_string(rawID);
                           *out++ = nodeID;
                           count++;
                         });
  return count; // Return the number of node IDs written
}

} // namespace AtlasNet