#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <optional>

namespace AtlasNet::SpatialObject
{

/**
 * Answers "which node owns this SpatialObject?" for intent routing.
 * Local store implements "owned here"; Redis/global can replace later.
 */
class IEntityOwnershipSource
{
public:
  virtual ~IEntityOwnershipSource() = default;

  [[nodiscard]] virtual std::optional<AtlasNetNodeID>
  OwnerOfEntity(const AtlasNetEntityID& id) const = 0;

  [[nodiscard]] virtual std::optional<AtlasNetNodeID>
  OwnerOfClient(const AtlasNetClientID& clientId) const = 0;
};

} // namespace AtlasNet::SpatialObject
