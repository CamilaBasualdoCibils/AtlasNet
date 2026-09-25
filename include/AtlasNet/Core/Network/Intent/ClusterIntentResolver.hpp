#pragma once

#include "AtlasNet/Core/Network/Intent/IIntentResolver.hpp"
#include "AtlasNet/Core/SpatialObject/IEntityOwnershipSource.hpp"
#include <optional>
#include <type_traits>
#include <variant>

namespace AtlasNet::Network::Intent
{

/**
 * Resolves intents using an optional SpatialObject ownership source.
 * Without a source, ResolveIntent returns nullopt (caller must not send).
 */
class ClusterIntentResolver : public IIntentResolver
{
public:
  ClusterIntentResolver() = default;

  explicit ClusterIntentResolver(
      SpatialObject::IEntityOwnershipSource* ownership)
      : ownership_(ownership)
  {
  }

  void SetOwnershipSource(SpatialObject::IEntityOwnershipSource* ownership)
  {
    ownership_ = ownership;
  }

  std::optional<AtlasNetNodeID>
  ResolveIntent(const VIntent& intent) override
  {
    if (!ownership_)
    {
      return std::nullopt;
    }

    return std::visit(
        [this](const auto& r) -> std::optional<AtlasNetNodeID>
        {
          using T = std::decay_t<decltype(r)>;
          if constexpr (std::is_same_v<T, Recepient::ShardOfEntityRecepient>)
          {
            return ownership_->OwnerOfEntity(r.entityID);
          }
          else if constexpr (std::is_same_v<T,
                                            Recepient::ShardOfClientRecepient>)
          {
            return ownership_->OwnerOfClient(r.clientID);
          }
          else if constexpr (std::is_same_v<T, Recepient::NodeRecepient>)
          {
            return r.nodeID;
          }
          else
          {
            return std::nullopt;
          }
        },
        intent);
  }

private:
  SpatialObject::IEntityOwnershipSource* ownership_ = nullptr;
};

} // namespace AtlasNet::Network::Intent
