

#pragma once

#include "ShardEnums.hpp"

#include "atlasnet/core/client/ClientDataEntry.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/entity/EntityHandle.hpp"
#include "atlasnet/core/entity/EntityLedger.hpp"
#include "atlasnet/core/node/AtlasNetNode.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include "atlasnet/shard/ShardRPC.hpp"
#include <iostream>
namespace AtlasNet
{
/**
 * @brief The IAtlasNetShard interface defines the required functions that any
 * shard must implement to interact with the AtlasNet core. This includes
 * entity registration, serialization, deserialization, and locking mechanisms.
 * The interface ensures consistent management and communication of shards
 * within the AtlasNet ecosystem.
 */
class IAtlasNetShard : public IAtlasNetNode
{
  std::optional<Entity::EntityLedger> _entityLedger;
  std::shared_ptr<spdlog::logger> _logger =
      spdlog::stdout_color_mt("AtlasNetShard");
  std::optional<AtlasNetEntityID::Generator> _entityIDGenerator;
  std::optional<AtlasNetShardID> shardID_;

public:
  IAtlasNetShard();
  virtual ~IAtlasNetShard() = default;

  AtlasNetShardID GetShardID() const
  {
    assert(shardID_.has_value() && "Shard ID not initialized");
    return shardID_.value();
  }

protected:
  virtual void OnShardInit() = 0;

private:
  void OnInit() override;

  void OnShutdown() override
  {
    GetLogger()->info("Shard OnShutdown called.");
    _entityLedger.reset();
  }

  ShardSpawnClientResponse
  impl_RPCSpawnClient(const ShardSpawnClientRequest& request);

  CommandAck HandleTransitCommand(const TransitCommandEnvelope& commandEnvelope,
                                  const RPCContext& context)
  {
    GetLogger()->info("Received transit command: {} from address: {}",
                      commandEnvelope.commandPackage.commandPayload.commandName,
                      context.sourceAddress.to_string());
    return CommandAck{CommandAckStatus::ServerAck};
  }

public:
  WorldID AtlasNet_GetWorldID();
  /**
   * @brief Registers a new entity created by the shard and returns its
   * EntityID. This should be called whenever the shard creates a new entity
   * that needs to be tracked by the AtlasNet system, so that the AtlasNet
   * system can track it and manage its lifecycle.
   *
   * @return EntityID new ID which must be kept by the shard and used for all
   * future references to this entity. The shard is responsible for ensuring the
   * uniqueness of this ID across all entities it creates, and for using this ID
   * in all subsequent interactions with the AtlasNet system regarding this
   * entity.
   */
  AtlasNetEntityID AtlasNet_RegisterEntity(Entity::Position transform);
  /**
   * @brief Deregister an existing local entity from the AtlasNet system. This
   * should be called when the entity in question should no longer be
   * transferable or tracked by the AtlasNet system, such as a local only entity
   * or when an entity is being destroyed.
   * @param id
   */
  void AtlasNet_UnregisterEntity(const AtlasNetEntityID& id);
  void AtlasNet_UpdateEntityTransform(const AtlasNetEntityID& id,
                                      const Entity::Position& transform);
  virtual void OnAtlasNetRequest_Shutdown() = 0; // Pure virtual function to be
                                                 // implemented by
                                                 // derived classes

  /**
   * @brief When this function is called, the shard should handle the detachment
   * of the entity with the given ID. The shard should use the provided
   * EntityDetachState to determine how to handle the detachment process. If the
   * state is Begin, the shard should freeze the entity, preventing any further
   * updates or interactions with it. If the state is Commit, the shard should
   * complete the detachment process, which may involve removing the entity from
   * the shard's internal tracking and resources. If the state is Cancel, the
   * shard should unfreeze the entity and keep it in the shard's internal
   * tracking and resources, effectively canceling the detachment process. The
   * shard is responsible for ensuring that the entity is properly handled
   * according to the specified state and that any necessary cleanup or resource
   * management is performed during the detachment process.
   *
   * @param state
   * @param id
   * @param remote_handle
   */
  virtual void OnDetachEntity(EntityDetachState state,
                              const AtlasNetEntityID& id,
                              const EntityHandle& remote_handle) = 0;
  /**
   * @brief When this function is called, the shard should serialize the entity
   * with the given ID and write its data to the provided ByteWriter. This
   * allows the AtlasNet system to capture the current state of the entity for
   * purposes such as saving, networking, or transferring between shards. The
   * shard is responsible for ensuring that all relevant data about the entity
   * is included in the serialization process, and that it is written in a
   * format that can be correctly deserialized later.
   *
   * @param id
   * @param writer
   */
  virtual void OnExportEntity(const AtlasNetEntityID& id,
                              ByteWriter& writer) = 0;

  /**
   * @brief When this function is called, the shard should deserialize the
   * entity with the given ID using the provided ByteWriter. This allows the
   * AtlasNet system to restore the state of the entity from previously captured
   * data.
   *
   * @param id
   * @param reader
   */
  virtual void OnAcquireEntity(const AtlasNetEntityID& id,
                               ByteReader& reader) = 0;
  // Received by the frontend of the shard
  struct ClientSpawnInfo
  {
    AtlasNetClientID clientID;
    AtlasNetEntityID entityID;
    Entity::Position position;
    std::vector<uint8_t>
        clientSpawnPayload; // This contains developer-defined data that will be
                            // given to the shard that spawns the client
  };
  virtual void OnSpawnClient(const ClientSpawnInfo& info) = 0;
};
} // namespace AtlasNet
