#pragma once
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/client/ClientDataEntry.hpp"
#include "atlasnet/core/database/redis/Redis.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/events/GlobalEventSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "enviroment/Enviroment.hpp"
namespace AtlasNet
{
class ClientRegistry
{
public:
  struct Config
  {
    GlobalEventSystem* _globalEventSystem;
    Database::RedisConn* __redisConn;
    AtlasNetClientID::Generator* _clientIDGenerator;
  };

  ClientRegistry(const Config& config) : config_(config)
  {
    assert(config_._globalEventSystem &&
           "GlobalEventSystem pointer cannot be null");
    assert(config_.__redisConn && "RedisConn pointer cannot be null");
    assert(config_._clientIDGenerator &&
           "ClientID generator pointer cannot be null");
  };

  std::optional<AtlasNetClientID> GetAddressClientID(const Network::SocketAddress& address)
  {

    ByteWriter addressWriter;
    address.Serialize(addressWriter);
    std::optional<std::string> value =
        config_.__redisConn->HashMap().GetSet().HGet(
            AddressToClientIDHashKey, addressWriter.as_string_view());
    if (!value)
    {
      return std::nullopt;
    }
    ByteReader reader(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(value->data()), value->size()));
    AtlasNetClientID clientID;
    reader(clientID);
    return clientID;
  }

  std::optional<Network::SocketAddress> GetClientIDAddress(const AtlasNetClientID& clientID)
  {
    ByteWriter clientIDWriter;
    clientIDWriter(clientID);
    std::optional<std::string> value =
        config_.__redisConn->HashMap().GetSet().HGet(
            ClientIDToAddressHashKey, clientIDWriter.as_string_view());
    if (!value)
    {
      return std::nullopt;
    }
    ByteReader reader(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(value->data()), value->size()));
    Network::SocketAddress address;
    address.Deserialize(reader);
    return address;
  }
  void AssociateClientWithEntity(const AtlasNetClientID& clientID,
                                 const AtlasNetEntityID& entityID)
  {
    ByteWriter clientIDWriter;
        clientIDWriter(clientID);

    ByteWriter entityIDWriter;
    entityIDWriter(entityID);

    assert(config_.__redisConn->HashMap().Exists().HExists(
               ClientID2EntityIDHashKey, clientIDWriter.as_string_view()) ==
               false &&
           "ClientID is already associated with an EntityID");

    assert(config_.__redisConn->HashMap().Exists().HExists(
               EntityID2ClientIDHashKey, entityIDWriter.as_string_view()) ==
               false &&
           "EntityID is already associated with a ClientID");
    config_.__redisConn->HashMap().GetSet().HSet(
        ClientID2EntityIDHashKey, clientIDWriter.as_string_view(),
        entityIDWriter.as_string_view());
    config_.__redisConn->HashMap().GetSet().HSet(
        EntityID2ClientIDHashKey, entityIDWriter.as_string_view(),
        clientIDWriter.as_string_view());

    if (Env::DebugMode)
    {
      config_.__redisConn->HashMap().GetSet().HSet(
          ClientID2EntityIDHashKey + "_debug", clientID.to_string(),
          entityID.to_string());
      config_.__redisConn->HashMap().GetSet().HSet(
          EntityID2ClientIDHashKey + "_debug", entityID.to_string(),
          clientID.to_string());
    }
  }
  struct LoginResult
  {

    AtlasNetClientID clientID;
    Entity::Location SpawnLocation;
    std::vector<uint8_t>
        SpawnShardPayload; // This contains developer-defined data that will be
                           // given to the shard that spawns the client
  };
  [[nodiscard]] std::optional<LoginResult>
  LoginClient(const Network::SocketAddress& address,AtlasNetGatewayID managingGatewayID)
  {

    std::optional<AtlasNetClientID> existingClientID = GetAddressClientID(address);
    if (existingClientID)
    {
        logger->error("Client with address {} is already logged in with ClientID: {}",
                     address.to_string(), existingClientID->to_string());
     
      return std::nullopt; // Address is already logged in
    }

    AtlasNetClientID newClientID = config_._clientIDGenerator->Next();
    ByteWriter addressWriter;
    LoginData entry;
    entry.address = address;
    entry.clientID = newClientID;
    entry.managingGateway = managingGatewayID;
    entry.address.Serialize(addressWriter);
    ByteWriter clientIDWriter;
    clientIDWriter(newClientID);

    config_.__redisConn->HashMap().GetSet().HSet(
        AddressToClientIDHashKey, addressWriter.as_string_view(),
        clientIDWriter.as_string_view());
    config_.__redisConn->HashMap().GetSet().HSet(
        ClientIDToAddressHashKey, clientIDWriter.as_string_view(),
        addressWriter.as_string_view());

    ByteWriter entryWriter;
    entry.Serialize(entryWriter);
    config_.__redisConn->HashMap().GetSet().HSet(
        ClientDataHashKey, clientIDWriter.as_string_view(),
        entryWriter.as_string_view());

    if (Env::DebugMode)
    {
      _Json j;
      entry.to_json(j);
      config_.__redisConn->HashMap().GetSet().HSet(
          ClientDataHashKey + "_Debug", entry.clientID.to_string(), j.dump());

      config_.__redisConn->HashMap().GetSet().HSet(
          AddressToClientIDHashKey + "_Debug", address.to_string(),
          newClientID.to_string());
      config_.__redisConn->HashMap().GetSet().HSet(
          ClientIDToAddressHashKey + "_Debug", newClientID.to_string(),
          address.to_string());
    }
    return LoginResult{newClientID, Entity::Location{}, std::vector<uint8_t>{}};
  }
  [[nodiscard]] std::optional<AtlasNetEntityID> GetClientEntityID(const AtlasNetClientID& clientID)
  {
    ByteWriter clientIDWriter;
    clientIDWriter(clientID);
    std::optional<std::string> value =
        config_.__redisConn->HashMap().GetSet().HGet(
            ClientID2EntityIDHashKey, clientIDWriter.as_string_view());
    if (!value)
    {
      return std::nullopt;
    }
    ByteReader reader(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(value->data()), value->size()));
    AtlasNetEntityID entityID;
    reader(entityID);
    return entityID;
  }
  [[nodiscard]] std::optional<AtlasNetClientID> GetClientIDFromEntityID(const AtlasNetEntityID& entityID)
  {
    ByteWriter entityIDWriter;
    entityIDWriter(entityID);
    std::optional<std::string> value =
        config_.__redisConn->HashMap().GetSet().HGet(
            EntityID2ClientIDHashKey, entityIDWriter.as_string_view());
    if (!value)
    {
      return std::nullopt;
    }
    ByteReader reader(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(value->data()), value->size()));
    AtlasNetClientID clientID;
    reader(clientID);
    return clientID;
  }

private:
std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("ClientRegistry");
  const Config config_;
  const std::string ClientRegistryNamespace =
      Env::DatabaseNamespace + "ClientRegistry{ATLASNET_CLIENT_REGISTRY}:";
  const std::string AddressToClientIDHashKey =
      ClientRegistryNamespace + "AddressToClientID";
  const std::string ClientIDToAddressHashKey =
      ClientRegistryNamespace + "ClientIDToAddress";
  const std::string ClientID2EntityIDHashKey =
      ClientRegistryNamespace + "ClientIDToEntityID";
  const std::string EntityID2ClientIDHashKey =
      ClientRegistryNamespace + "EntityIDToClientID";
  const std::string ClientDataHashKey = ClientRegistryNamespace + "ClientData";
};
} // namespace AtlasNet