#pragma once

#include "AtlasNet/Core/Events/IGlobalEvents.hpp"
#include <sw/redis++/async_redis.h>
#include <sw/redis++/async_subscriber.h>
namespace AtlasNet::Events
{
class RedisGlobalEvents : public IGlobalEvents
{
public:
  RedisGlobalEvents(std::shared_ptr<sw::redis::AsyncRedis> redisClient)
      : redisClient(redisClient), redisSubscriber(redisClient->subscriber())
  {
    redisSubscriber.on_message(
        [this](const std::string& channel, const std::string& message)
        { __DispatchEvent(channel, message); });
  }

private:
  void __SubscribeToEvent(const std::string& eventName) override
  {
    redisSubscriber.subscribe(eventName);
  }

  void __UnsubscribeFromEvent(const std::string& eventName) override
  {
    redisSubscriber.unsubscribe(eventName);
  }

  std::shared_ptr<sw::redis::AsyncRedis> redisClient;
  sw::redis::AsyncSubscriber redisSubscriber;
};
}; // namespace AtlasNet::Events