#pragma once

#include "AtlasNet/Core/SpatialObject/IEntityOwnershipSource.hpp"
#include "AtlasNet/Core/SpatialObject/ISpatialObjectIndex.hpp"
#include "AtlasNet/Core/SpatialObject/ISpatialObjectStore.hpp"
#include <mutex>
#include <unordered_map>

namespace AtlasNet::SpatialObject
{

/**
 * Default composing store: authority-thread mutations on Backend + publish
 * into a lock-free index for concurrent readers.
 */
template <typename BackendT>
class SpatialObjectStore final : public ISpatialObjectStore
{
public:
  SpatialObjectStore(ISpatialObjectIndex& index, BackendT& backend,
                     AtlasNetNodeID localNodeId = {})
      : index_(index), backend_(backend), localNodeId_(localNodeId)
  {
  }

  bool SpawnActor(const SpawnActorRequest& request) override
  {
    std::lock_guard lock(mutex_);
    if (index_.Contains(request.id) || entities_.contains(request.id))
    {
      return false;
    }
    const BackendEntity be = backend_.Create();
    backend_.template Emplace<ObjectInfo>(be, request.info);
    backend_.template Emplace<Transform>(be, request.transform);
    backend_.template Emplace<ActorTag>(be);

    PublishedState published = MakePublished(request.id, request.info,
                                             request.transform, true, false,
                                             {}, 0);
    if (index_.Insert(request.id, published) != InsertResult::Ok)
    {
      backend_.Destroy(be);
      return false;
    }
    entities_[request.id] = be;
    sequences_[request.id] = 0;
    return true;
  }

  bool SpawnClientActor(const SpawnClientActorRequest& request) override
  {
    std::lock_guard lock(mutex_);
    if (index_.Contains(request.id) || entities_.contains(request.id))
    {
      return false;
    }
    const BackendEntity be = backend_.Create();
    backend_.template Emplace<ObjectInfo>(be, request.info);
    backend_.template Emplace<Transform>(be, request.transform);
    backend_.template Emplace<ActorTag>(be);
    backend_.template Emplace<ClientInfo>(be, ClientInfo{request.clientId});

    PublishedState published = MakePublished(
        request.id, request.info, request.transform, true, true,
        request.clientId, 0);
    if (index_.Insert(request.id, published) != InsertResult::Ok)
    {
      backend_.Destroy(be);
      return false;
    }
    entities_[request.id] = be;
    clientToEntity_[request.clientId] = request.id;
    sequences_[request.id] = 0;
    return true;
  }

  bool Despawn(const AtlasNetEntityID& id) override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return false;
    }
    if (auto* client = backend_.template TryGet<ClientInfo>(it->second))
    {
      clientToEntity_.erase(client->clientId);
    }
    backend_.Destroy(it->second);
    entities_.erase(it);
    sequences_.erase(id);
    index_.Remove(id);
    return true;
  }

  bool UpdateTransform(const AtlasNetEntityID& id,
                       const Transform& transform) override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return false;
    }
    if (auto* t = backend_.template TryGet<Transform>(it->second))
    {
      *t = transform;
    }
    else
    {
      backend_.template Emplace<Transform>(it->second, transform);
    }
    return RepublishLocked(id);
  }

  [[nodiscard]] bool Exists(const AtlasNetEntityID& id) const override
  {
    std::lock_guard lock(mutex_);
    return entities_.contains(id);
  }

  [[nodiscard]] bool IsActor(const AtlasNetEntityID& id) const override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return false;
    }
    return backend_.template Has<ActorTag>(it->second);
  }

  [[nodiscard]] bool IsClient(const AtlasNetEntityID& id) const override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return false;
    }
    return backend_.template Has<ClientInfo>(it->second);
  }

  [[nodiscard]] std::optional<PublishedState>
  ReadPublished(const AtlasNetEntityID& id) const override
  {
    return index_.ReadPublished(id);
  }

  [[nodiscard]] std::optional<Transform>
  GetTransform(const AtlasNetEntityID& id) const override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return std::nullopt;
    }
    if (const auto* t = backend_.template TryGet<Transform>(it->second))
    {
      return *t;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<ObjectInfo>
  GetObjectInfo(const AtlasNetEntityID& id) const override
  {
    std::lock_guard lock(mutex_);
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return std::nullopt;
    }
    if (const auto* info = backend_.template TryGet<ObjectInfo>(it->second))
    {
      return *info;
    }
    return std::nullopt;
  }

  std::optional<uint64_t>
  NextCommandSequence(const AtlasNetEntityID& id) override
  {
    std::lock_guard lock(mutex_);
    auto it = sequences_.find(id);
    if (it == sequences_.end())
    {
      return std::nullopt;
    }
    ++it->second;
    RepublishLocked(id);
    return it->second;
  }

  [[nodiscard]] IComponentBackend& Backend() override
  {
    return backend_;
  }
  [[nodiscard]] const IComponentBackend& Backend() const override
  {
    return backend_;
  }

  [[nodiscard]] AtlasNetNodeID LocalNodeId() const
  {
    return localNodeId_;
  }

  [[nodiscard]] std::optional<AtlasNetEntityID>
  EntityForClient(const AtlasNetClientID& clientId) const
  {
    std::lock_guard lock(mutex_);
    auto it = clientToEntity_.find(clientId);
    if (it == clientToEntity_.end())
    {
      return std::nullopt;
    }
    return it->second;
  }

private:
  static PublishedState MakePublished(const AtlasNetEntityID& id,
                                      const ObjectInfo& info,
                                      const Transform& transform, bool isActor,
                                      bool isClient,
                                      const AtlasNetClientID& clientId,
                                      uint64_t sequence)
  {
    PublishedState p;
    p.id = id;
    p.worldId = info.worldId;
    p.coordinateSystem = transform.coordinateSystem;
    p.x = transform.x;
    p.y = transform.y;
    p.z = transform.z;
    p.roles.isActor = isActor;
    p.roles.isClient = isClient;
    p.roles.alive = true;
    p.clientId = clientId;
    p.commandSequence = sequence;
    return p;
  }

  bool RepublishLocked(const AtlasNetEntityID& id)
  {
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
      return false;
    }
    const auto* info = backend_.template TryGet<ObjectInfo>(it->second);
    const auto* transform = backend_.template TryGet<Transform>(it->second);
    if (!info || !transform)
    {
      return false;
    }
    AtlasNetClientID clientId{};
    const bool isClient = backend_.template Has<ClientInfo>(it->second);
    if (isClient)
    {
      if (const auto* c = backend_.template TryGet<ClientInfo>(it->second))
      {
        clientId = c->clientId;
      }
    }
    const uint64_t seq = sequences_[id];
    PublishedState published =
        MakePublished(id, *info, *transform,
                      backend_.template Has<ActorTag>(it->second), isClient,
                      clientId, seq);
    return index_.Publish(id, published);
  }

  ISpatialObjectIndex& index_;
  BackendT& backend_;
  AtlasNetNodeID localNodeId_;
  mutable std::mutex mutex_;
  std::unordered_map<AtlasNetEntityID, BackendEntity> entities_;
  std::unordered_map<AtlasNetClientID, AtlasNetEntityID> clientToEntity_;
  std::unordered_map<AtlasNetEntityID, uint64_t> sequences_;
};

/** Ownership source backed by a local SpatialObjectStore. */
template <typename BackendT>
class StoreOwnershipSource final : public IEntityOwnershipSource
{
public:
  StoreOwnershipSource(const SpatialObjectStore<BackendT>& store,
                       AtlasNetNodeID localNode)
      : store_(store), localNode_(localNode)
  {
  }

  [[nodiscard]] std::optional<AtlasNetNodeID>
  OwnerOfEntity(const AtlasNetEntityID& id) const override
  {
    if (store_.Exists(id))
    {
      return localNode_;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<AtlasNetNodeID>
  OwnerOfClient(const AtlasNetClientID& clientId) const override
  {
    if (store_.EntityForClient(clientId))
    {
      return localNode_;
    }
    return std::nullopt;
  }

private:
  const SpatialObjectStore<BackendT>& store_;
  AtlasNetNodeID localNode_;
};

} // namespace AtlasNet::SpatialObject
