#pragma once

#include "AtlasNet/Core/SpatialObject/Components.hpp"
#include "AtlasNet/Core/SpatialObject/IComponentBackend.hpp"
#include <entt/entt.hpp>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace AtlasNet::SpatialObject
{

/**
 * EnTT-backed component storage. Authority-thread only for mutate/view.
 * Typed helpers live on this concrete type (not on IComponentBackend).
 */
class EnttComponentBackend final : public IComponentBackend
{
public:
  BackendEntity Create() override
  {
    const entt::entity e = registry_.create();
    const auto id = static_cast<BackendEntity>(
        static_cast<std::underlying_type_t<entt::entity>>(e));
    return id;
  }

  void Destroy(BackendEntity entity) override
  {
    const entt::entity e = ToEntt(entity);
    if (registry_.valid(e))
    {
      registry_.destroy(e);
    }
  }

  [[nodiscard]] bool Valid(BackendEntity entity) const override
  {
    return registry_.valid(ToEntt(entity));
  }

  template <typename Component, typename... Args>
  Component& Emplace(BackendEntity entity, Args&&... args)
  {
    return registry_.emplace<Component>(ToEntt(entity),
                                        std::forward<Args>(args)...);
  }

  template <typename Component>
  void Remove(BackendEntity entity)
  {
    registry_.remove<Component>(ToEntt(entity));
  }

  template <typename Component>
  [[nodiscard]] bool Has(BackendEntity entity) const
  {
    return registry_.all_of<Component>(ToEntt(entity));
  }

  template <typename Component>
  [[nodiscard]] Component* TryGet(BackendEntity entity)
  {
    return registry_.try_get<Component>(ToEntt(entity));
  }

  template <typename Component>
  [[nodiscard]] const Component* TryGet(BackendEntity entity) const
  {
    return registry_.try_get<Component>(ToEntt(entity));
  }

  [[nodiscard]] entt::registry& Registry()
  {
    return registry_;
  }
  [[nodiscard]] const entt::registry& Registry() const
  {
    return registry_;
  }

private:
  static entt::entity ToEntt(BackendEntity entity)
  {
    return static_cast<entt::entity>(entity);
  }

  entt::registry registry_;
};

/**
 * Minimal fake backend for seam tests (no EnTT).
 */
class MapComponentBackend final : public IComponentBackend
{
public:
  BackendEntity Create() override
  {
    const BackendEntity id = next_++;
    alive_.insert(id);
    return id;
  }

  void Destroy(BackendEntity entity) override
  {
    alive_.erase(entity);
    objectInfo_.erase(entity);
    transform_.erase(entity);
    actors_.erase(entity);
    clients_.erase(entity);
  }

  [[nodiscard]] bool Valid(BackendEntity entity) const override
  {
    return alive_.contains(entity);
  }

  template <typename Component>
  Component& Emplace(BackendEntity entity, const Component& component)
  {
    if constexpr (std::is_same_v<Component, ObjectInfo>)
    {
      return objectInfo_[entity] = component;
    }
    else if constexpr (std::is_same_v<Component, Transform>)
    {
      return transform_[entity] = component;
    }
    else if constexpr (std::is_same_v<Component, ActorTag>)
    {
      actors_.insert(entity);
      return actorTags_[entity];
    }
    else if constexpr (std::is_same_v<Component, ClientInfo>)
    {
      return clients_[entity] = component;
    }
    else
    {
      static_assert(sizeof(Component) == 0, "unsupported component");
    }
  }

  template <typename Component, typename... Args>
    requires(sizeof...(Args) > 0 && !std::is_same_v<Component, ActorTag>)
  Component& Emplace(BackendEntity entity, Args&&... args)
  {
    return Emplace<Component>(entity, Component{std::forward<Args>(args)...});
  }

  template <typename Component>
    requires std::is_same_v<Component, ActorTag>
  Component& Emplace(BackendEntity entity)
  {
    actors_.insert(entity);
    return actorTags_[entity];
  }

  template <typename Component>
  [[nodiscard]] bool Has(BackendEntity entity) const
  {
    if constexpr (std::is_same_v<Component, ObjectInfo>)
    {
      return objectInfo_.contains(entity);
    }
    else if constexpr (std::is_same_v<Component, Transform>)
    {
      return transform_.contains(entity);
    }
    else if constexpr (std::is_same_v<Component, ActorTag>)
    {
      return actors_.contains(entity);
    }
    else if constexpr (std::is_same_v<Component, ClientInfo>)
    {
      return clients_.contains(entity);
    }
    else
    {
      return false;
    }
  }

  template <typename Component>
  [[nodiscard]] Component* TryGet(BackendEntity entity)
  {
    if constexpr (std::is_same_v<Component, ObjectInfo>)
    {
      auto it = objectInfo_.find(entity);
      return it == objectInfo_.end() ? nullptr : &it->second;
    }
    else if constexpr (std::is_same_v<Component, Transform>)
    {
      auto it = transform_.find(entity);
      return it == transform_.end() ? nullptr : &it->second;
    }
    else if constexpr (std::is_same_v<Component, ClientInfo>)
    {
      auto it = clients_.find(entity);
      return it == clients_.end() ? nullptr : &it->second;
    }
    else
    {
      return nullptr;
    }
  }

  template <typename Component>
  [[nodiscard]] const Component* TryGet(BackendEntity entity) const
  {
    return const_cast<MapComponentBackend*>(this)->TryGet<Component>(entity);
  }

private:
  BackendEntity next_ = 1;
  std::unordered_set<BackendEntity> alive_;
  std::unordered_set<BackendEntity> actors_;
  std::unordered_map<BackendEntity, ObjectInfo> objectInfo_;
  std::unordered_map<BackendEntity, Transform> transform_;
  std::unordered_map<BackendEntity, ClientInfo> clients_;
  std::unordered_map<BackendEntity, ActorTag> actorTags_;
};

} // namespace AtlasNet::SpatialObject
