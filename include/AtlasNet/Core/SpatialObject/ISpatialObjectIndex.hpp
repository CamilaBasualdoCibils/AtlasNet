#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/SpatialObject/Types.hpp"
#include <optional>

namespace AtlasNet::SpatialObject
{

class ISpatialObjectIndex
{
public:
  virtual ~ISpatialObjectIndex() = default;

  virtual InsertResult Insert(const AtlasNetEntityID& id,
                              const PublishedState& initial) = 0;
  virtual RemoveResult Remove(const AtlasNetEntityID& id) = 0;
  virtual RemoveResult Remove(SpatialObjectHandle handle) = 0;

  [[nodiscard]] virtual std::optional<SpatialObjectHandle>
  Find(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual bool Contains(const AtlasNetEntityID& id) const = 0;

  /** Seqlock read of published hot state. */
  [[nodiscard]] virtual std::optional<PublishedState>
  ReadPublished(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual std::optional<PublishedState>
  ReadPublished(SpatialObjectHandle handle) const = 0;

  /** Authority publish: bumps seqlock version around the write. */
  virtual bool Publish(const AtlasNetEntityID& id,
                       const PublishedState& state) = 0;

  [[nodiscard]] virtual std::size_t Size() const = 0;
  [[nodiscard]] virtual std::size_t Capacity() const = 0;
};

} // namespace AtlasNet::SpatialObject
