#pragma once

#include "EntityLedgerRPC.hpp"
#include "atlasnet/core/RPC/RPCSystem.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "entt/entity/fwd.hpp"
#include <boost/bimap.hpp>
#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
namespace AtlasNet
{
namespace Entity
{

class EntityLedger
{
  using EnTTEntityID = EntityTable::entity_type;

public:
  enum ActorTransferMode
  {
    eRPC,
    eMessage
  };
  struct Config
  {
    ActorTransferMode transferMode = ActorTransferMode::eRPC;
    RPCSystem* rpcSystem = nullptr;
    AtlasNetEntityID::Generator* entityIDGenerator = nullptr;
  };
  EntityLedger(const Config& config)
      : _config(config)
  {
    assert(_config.rpcSystem != nullptr && "RPCSystem pointer cannot be null in EntityLedger config");
    assert(_config.entityIDGenerator != nullptr && "EntityID generator pointer cannot be null in EntityLedger config");
    SetRPCBinds();
  }

  class ReadAccess
  {
  public:
    explicit ReadAccess(const EntityLedger& ledger)
        : _ledger(ledger), _lock(ledger._mutex)
    {
    }

    bool EntityExists(const AtlasNetEntityID& id) const
    {
      return _ledger._entityExists(id);
    }

    bool IsClient(const AtlasNetEntityID& id) const
    {
      return _ledger._isClient(id);
    }
    bool IsActor(const AtlasNetEntityID& id) const
    {
      return _ledger._isActor(id);
    }
    Entity::Components::EntityInfo GetEntityInfo(const AtlasNetEntityID& id) const
    {
      return _ledger._GetEntityInfo(id);
    }

  private:
    const EntityLedger& _ledger;
    std::shared_lock<std::shared_mutex> _lock;
  };
  class WriteAccess
  {
  public:
    explicit WriteAccess(EntityLedger& ledger)
        : _ledger(ledger), _lock(ledger._mutex)
    {
    }

    bool EntityExists(const AtlasNetEntityID& id) const
    {
      return _ledger._entityExists(id);
    }

    bool IsClient(const AtlasNetEntityID& id) const
    {
      return _ledger._isClient(id);
    }
    bool IsActor(const AtlasNetEntityID& id) const
    {
      return _ledger._isActor(id);
    }

    AtlasNetEntityID CreateEntity(const Components::BaseEntityInfo& info)
    {
      AtlasNetEntityID id = _ledger._config.entityIDGenerator->Next();
      Entity::Components::EntityInfo entityInfo;
      entityInfo.baseInfo = info;
      entityInfo.id = id;
      _ledger._createEntity(id, entityInfo);
      return id;
    }
    void RemoveEntity(const AtlasNetEntityID& id)
    {
      _ledger._removeEntity(id);
    }
    Entity::Components::EntityInfo GetEntityInfo(const AtlasNetEntityID& id)
    {
      return _ledger._GetEntityInfo(id);
    }
    void SetEntityInfo(const AtlasNetEntityID& id,
                       const Entity::Components::EntityInfo& info)
    {
      _ledger._SetEntityInfo(id, info);
    }

  private:
    EntityLedger& _ledger;
    std::unique_lock<std::shared_mutex> _lock;
  };
  WriteAccess GetWriteAccess()
  {
    return WriteAccess(*this);
  }
  ReadAccess GetReadAccess() const
  {
    return ReadAccess(*this);
  }

protected:
  EnTTEntityID _createEntity(const AtlasNetEntityID& id,
                             const Entity::Components::EntityInfo& info)
  {
    EnTTEntityID enttId = entityTable.create();
    IDMapping.insert({id, enttId});
    entityTable.emplace<Entity::Components::EntityInfo>(enttId, info);
    logger->info("Entity created with ID: {} with internal entt ID: {}", id.to_string(), static_cast<int>(enttId));
    return enttId;
  }
  bool _entityExists(const AtlasNetEntityID& id) const
  {
    return IDMapping.left.find(id) != IDMapping.left.end();
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  bool _EntityHasComponent(const AtlasNetEntityID& id) const
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    return entityTable.all_of<ComponentType>(enttId);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  void _EntityAddComponent(const AtlasNetEntityID& id, const ComponentType& component)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity already has component");
    }
    entityTable.emplace<ComponentType>(enttId, component);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  void _EntityRemoveComponent(const AtlasNetEntityID& id)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (!entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity does not have component");
    }
    entityTable.remove<ComponentType>(enttId);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  ComponentType _EntityGetComponent(const AtlasNetEntityID& id)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (!entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity does not have component");
    }
    return entityTable.get<ComponentType>(enttId);
  }
  bool _isClient(const AtlasNetEntityID& id) const
  {
    return _EntityHasComponent<Entity::Components::ClientInfo>(id);
  }
  bool _isActor(const AtlasNetEntityID& id) const
  {
    return _EntityHasComponent<Entity::Components::ActorInfo>(id);
  }
  void _setEntityActor(const AtlasNetEntityID& id,
                       const Entity::Components::ActorInfo& actorInfo)
  {
    _EntityAddComponent(id, actorInfo);
  }
  void _setEntityClient(const AtlasNetEntityID& id,
                        const Entity::Components::ClientInfo& clientInfo)
  {
    _EntityAddComponent(id, clientInfo);
  }
  void _removeEntity(const AtlasNetEntityID& id)
  {
    entt::entity enttId = GetEnTTEntityID(id);
    entityTable.destroy(enttId);
    IDMapping.left.erase(id);
  }

  Entity::Components::EntityInfo _GetEntityInfo(const AtlasNetEntityID& id) const
  {
    EnTTEntityID _id = GetEnTTEntityID(id);
    auto& info = entityTable.get<Entity::Components::EntityInfo>(_id);
    // Use info as needed
    return info;
  }
  void _SetEntityInfo(const AtlasNetEntityID& id,
                      const Entity::Components::EntityInfo& info)
  {
    EnTTEntityID _id = GetEnTTEntityID(id);
    auto& entityInfo = entityTable.get<Entity::Components::EntityInfo>(_id);
    entityInfo = info;
  }

private:
void SetRPCBinds();
  [[nodiscard]] EnTTEntityID GetEnTTEntityID(const AtlasNetEntityID& id) const
  {
    auto it = IDMapping.left.find(id);
    if (it != IDMapping.left.end())
    {
      return it->second;
    }
    throw std::runtime_error("EntityID not found in mapping");
  }
  std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("EntityLedger");
  const Config _config;
  boost::bimap<AtlasNetEntityID, EnTTEntityID> IDMapping;
  // std::unordered_map<EntityID, typename Tp>
  EntityTable entityTable;
  mutable std::shared_mutex _mutex;

};
} // namespace Entity
} // namespace AtlasNet