#pragma once

#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "boost/describe/enum.hpp"
#include "sw/redis++/async_redis.h"
#include "sw/redis++/async_redis_cluster.h"
#include "sw/redis++/async_subscriber.h"
#include "sw/redis++/command_options.h"
#include "sw/redis++/connection.h"
#include "sw/redis++/connection_pool.h"
#include "sw/redis++/redis.h"
#include "sw/redis++/redis_cluster.h"
#include "sw/redis++/subscriber.h"
#include <chrono>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sw/redis++/redis++.h>
#include <sys/types.h>
#include <variant>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace AtlasNet::Database
{
namespace Redis
{
class KeyValWrapper;
class HashMapWrapper;
class SetWrapper;
class SortedSetWrapper;
} // namespace Redis

class RedisConn
{
  friend class Redis::KeyValWrapper;
  friend class Redis::HashMapWrapper;
  friend class Redis::SetWrapper;
  friend class Redis::SortedSetWrapper;

public:
  enum class RedisMode
  {
    eCluster,
    eStandalone
  };
  BOOST_DESCRIBE_NESTED_ENUM(RedisMode, eCluster, eStandalone);
  struct Settings
  {

    Network::HostAddress host;
    Network::PortType port;
    RedisMode Mode = RedisMode::eStandalone;
    bool ExceptionOnFailure = false;
    uint32_t MaxConnectRetries = 0;
    std::chrono::milliseconds ConnectRetryDelay{1000};

    std::string user = "default";
    std::string password;
    std::optional<std::chrono::milliseconds>
        SocketTimeout; // If not set then never timeout. block until response is
                       // received.
    std::optional<std::chrono::milliseconds>
        ConnectTimeout; // If not set then never timeout. block until connection
                        // is established.
    uint16_t PoolSize = 10;
    std::optional<std::chrono::milliseconds>
        PoolConnectionIdleTimeout; // If not set then never timeout. block until
                                   // connection is established.
  } settings;

private:
  std::variant<sw::redis::Redis, sw::redis::RedisCluster> HandleVariant;
  std::variant<sw::redis::AsyncRedis, sw::redis::AsyncRedisCluster> AsyncHandleVariant;

  template <typename Func> decltype(auto) RedisFunc(Func&& func)
  {
    return std::visit(
        [&](auto&& handle) -> decltype(auto)
        {
          return std::invoke(std::forward<Func>(func),
                             std::forward<decltype(handle)>(handle));
        },
        HandleVariant);
  }
  template <typename Func> decltype(auto) AsyncRedisFunc(Func&& func)
  {
    return std::visit(
        [&](auto&& handle) -> decltype(auto)
        {
          return std::invoke(std::forward<Func>(func),
                             std::forward<decltype(handle)>(handle));
        },
        AsyncHandleVariant);
  }

public:
  RedisConn(sw::redis::Redis redis, sw::redis::AsyncRedis aredis,
            const Settings& settings);

  RedisConn(sw::redis::RedisCluster redis, sw::redis::AsyncRedisCluster aredis,
            const Settings& settings);
  ~RedisConn();

  static std::unique_ptr<RedisConn> Connect(const Settings& settings);

  Redis::KeyValWrapper& KeyVal();
  Redis::HashMapWrapper& HashMap();
  Redis::SetWrapper& Set();
  Redis::SortedSetWrapper& SortedSet();
  template <typename Result, typename Input>
  std::optional<Result> Command(Input first, Input last)
  {
    return RedisFunc([&](auto& handle) -> Result
                     { return handle.template command<Result>(first, last); });
  }
  template <typename Input, typename Output>
  void Command(Input first, Input last, Output output)
  {
    return RedisFunc([&](auto& handle) -> void
                     { return handle.command(first, last, output); });
  }
  sw::redis::Subscriber Subscribe()
  {
    return RedisFunc([&](auto& handle) { return handle.subscriber(); });
  }
  sw::redis::AsyncSubscriber AsyncSubscribe()
  {
    return AsyncRedisFunc([&](auto& handle) { return handle.subscriber(); });
  }

  void Publish(const std::string& channel, const std::string_view& message)
  {
    RedisFunc([&](auto& handle) { handle.publish(channel, message); });
  }

private:
  std::unique_ptr<Redis::KeyValWrapper> keyValWrapper;
  std::unique_ptr<Redis::HashMapWrapper> hashMapWrapper;
  std::unique_ptr<Redis::SetWrapper> setWrapper;
  std::unique_ptr<Redis::SortedSetWrapper> sortedSetWrapper;
  static inline std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("Redis");
};

} // namespace AtlasNet::Database