#pragma once

#include "AtlasNet/Core/SpatialObject/Components.hpp"
#include <cstdint>
#include <optional>
#include <typeindex>

namespace AtlasNet::SpatialObject
{

/** Opaque entity id inside a component backend (not the AtlasNet Snowflake). */
using BackendEntity = uint32_t;

inline constexpr BackendEntity kInvalidBackendEntity =
    static_cast<BackendEntity>(-1);

/**
 * Swappable dense component storage.
 * Typed access is provided by concrete backends (e.g. EnTT); this interface
 * covers lifecycle only so callers can depend on an abstraction.
 */
class IComponentBackend
{
public:
  virtual ~IComponentBackend() = default;

  virtual BackendEntity Create() = 0;
  virtual void Destroy(BackendEntity entity) = 0;
  [[nodiscard]] virtual bool Valid(BackendEntity entity) const = 0;
};

} // namespace AtlasNet::SpatialObject
