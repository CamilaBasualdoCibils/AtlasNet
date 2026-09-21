#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/SpatialObject/Components.hpp"
#include "AtlasNet/Core/SpatialObject/IComponentBackend.hpp"
#include "AtlasNet/Core/SpatialObject/Types.hpp"
#include <optional>

namespace AtlasNet::SpatialObject
{

struct SpawnActorRequest
{
  AtlasNetEntityID id{};
  ObjectInfo info{};
  Transform transform{};
};

struct SpawnClientActorRequest
{
  AtlasNetEntityID id{};
  ObjectInfo info{};
  Transform transform{};
  AtlasNetClientID clientId{};
};

class ISpatialObjectStore
{
public:
  virtual ~ISpatialObjectStore() = default;

  virtual bool SpawnActor(const SpawnActorRequest& request) = 0;
  virtual bool SpawnClientActor(const SpawnClientActorRequest& request) = 0;
  virtual bool Despawn(const AtlasNetEntityID& id) = 0;

  virtual bool UpdateTransform(const AtlasNetEntityID& id,
                               const Transform& transform) = 0;

  [[nodiscard]] virtual bool Exists(const AtlasNetEntityID& id) const = 0;
  [[nodiscard]] virtual bool IsActor(const AtlasNetEntityID& id) const = 0;
  [[nodiscard]] virtual bool IsClient(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual std::optional<PublishedState>
  ReadPublished(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual std::optional<Transform>
  GetTransform(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual std::optional<ObjectInfo>
  GetObjectInfo(const AtlasNetEntityID& id) const = 0;

  /** Bump and return next command sequence for this actor (authority). */
  virtual std::optional<uint64_t>
  NextCommandSequence(const AtlasNetEntityID& id) = 0;

  [[nodiscard]] virtual IComponentBackend& Backend() = 0;
  [[nodiscard]] virtual const IComponentBackend& Backend() const = 0;
};

} // namespace AtlasNet::SpatialObject
