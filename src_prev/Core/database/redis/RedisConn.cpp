#include "atlasnet/core/database/redis/RedisConn.hpp"

#include "atlasnet/core/utils/assert.hpp"
#include "atlasnet/core/database/redis/HashMapWrapper.hpp"
#include "atlasnet/core/database/redis/KeyValWrapper.hpp"
#include "atlasnet/core/database/redis/SetWrapper.hpp"
#include "atlasnet/core/database/redis/SortedSetWrapper.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "sw/redis++/async_redis_cluster.h"
#include <iostream>
std::unique_ptr<AtlasNet::Database::RedisConn>
AtlasNet::Database::RedisConn::Connect(const Settings& settings)
{
  sw::redis::ConnectionOptions opts;
  opts.host = settings.host.to_string();
  opts.port = settings.port;
  if (settings.SocketTimeout)
  {
    opts.socket_timeout = *settings.SocketTimeout;
  }
  if (settings.ConnectTimeout)
  {
    opts.connect_timeout = *settings.ConnectTimeout;
  }

  opts.user = settings.user;
  opts.password = settings.password;
  sw::redis::ConnectionPoolOptions pool_opts;
  pool_opts.size = settings.PoolSize;
  if (settings.PoolConnectionIdleTimeout)
  {
    pool_opts.connection_idle_time = *settings.PoolConnectionIdleTimeout;
  }
  logger->info("Attempting to connect to Redis at {}:{} in {} mode with up to {} retries...",
               opts.host, opts.port,
               boost::describe::enum_to_string(settings.Mode, "UNKNOWN MODE"),
               settings.MaxConnectRetries);
  
  for (uint32_t attempt = 1; attempt <= settings.MaxConnectRetries; ++attempt)
  {
    try
    {
      if (settings.Mode == RedisMode::eCluster)
      {
        sw::redis::RedisCluster cluster(opts, pool_opts);

        // REAL validation: must touch cluster directly
        auto replies = cluster.redis("PING").ping();

        if (replies.empty())
        {
          throw sw::redis::Error("Cluster PING returned no responses");
        }
        logger->info("Successfully connected to Redis in Cluster mode.");


        auto acluster = sw::redis::AsyncRedisCluster(opts, pool_opts);

        return std::make_unique<RedisConn>(std::move(cluster),
                                           std::move(acluster), settings);
      }
      else
      {
        sw::redis::Redis redis(opts, pool_opts);

        // REAL validation: must be a round-trip
        auto pong = redis.ping();
        if (pong != "PONG")
        {
          throw sw::redis::Error("Standalone PING did not return PONG");
        }

        logger->info("Successfully connected to Redis in Standalone mode.");

        auto aredis = sw::redis::AsyncRedis(opts, pool_opts);

        return std::make_unique<RedisConn>(std::move(redis), std::move(aredis),
                                           settings);
      }
    }
    catch (const sw::redis::Error& e)
    {
      logger->error("Attempt {}/{}: Failed to connect to Redis: {}",
                    attempt, settings.MaxConnectRetries, e.what());
      if (attempt < settings.MaxConnectRetries)
      {
        std::this_thread::sleep_for(settings.ConnectRetryDelay);
      }
    }
  }
  logger->error("Failed to connect to Redis at {}:{} in {} mode after {} attempts",
                opts.host, opts.port,
                boost::describe::enum_to_string(settings.Mode, "UNKNOWN MODE"),
                settings.MaxConnectRetries);
        
  return nullptr;
}
AtlasNet::Database::Redis::KeyValWrapper&
AtlasNet::Database::RedisConn::KeyVal()
{
  AN_ASSERT(keyValWrapper, "KeyValWrapper is not initialized");
  return *keyValWrapper;
}
AtlasNet::Database::Redis::HashMapWrapper&
AtlasNet::Database::RedisConn::HashMap()
{
  AN_ASSERT(hashMapWrapper, "HashMapWrapper is not initialized");
  return *hashMapWrapper;
}
AtlasNet::Database::Redis::SetWrapper& AtlasNet::Database::RedisConn::Set()
{
  AN_ASSERT(setWrapper, "SetWrapper is not initialized");
  return *setWrapper;
}
AtlasNet::Database::Redis::SortedSetWrapper&
AtlasNet::Database::RedisConn::SortedSet()
{
  AN_ASSERT(sortedSetWrapper, "SortedSetWrapper is not initialized");
  return *sortedSetWrapper;
}
AtlasNet::Database::RedisConn::~RedisConn() {}
AtlasNet::Database::RedisConn::RedisConn(sw::redis::Redis redis,
                                         sw::redis::AsyncRedis aredis,
                                         const Settings& settings)
    : settings(settings), HandleVariant(std::move(redis)),
      AsyncHandleVariant(std::move(aredis))
{
  keyValWrapper =
      std::unique_ptr<Redis::KeyValWrapper>(new Redis::KeyValWrapper(*this));
  hashMapWrapper =
      std::unique_ptr<Redis::HashMapWrapper>(new Redis::HashMapWrapper(*this));
  setWrapper = std::unique_ptr<Redis::SetWrapper>(new Redis::SetWrapper(*this));
  sortedSetWrapper = std::unique_ptr<Redis::SortedSetWrapper>(
      new Redis::SortedSetWrapper(*this));
}
AtlasNet::Database::RedisConn::RedisConn(sw::redis::RedisCluster redis,
                                         sw::redis::AsyncRedisCluster aredis,
                                         const Settings& settings)
    : settings(settings), HandleVariant(std::move(redis)),
      AsyncHandleVariant(std::move(aredis))
{
  keyValWrapper =
      std::unique_ptr<Redis::KeyValWrapper>(new Redis::KeyValWrapper(*this));
  hashMapWrapper =
      std::unique_ptr<Redis::HashMapWrapper>(new Redis::HashMapWrapper(*this));
  setWrapper = std::unique_ptr<Redis::SetWrapper>(new Redis::SetWrapper(*this));
  sortedSetWrapper = std::unique_ptr<Redis::SortedSetWrapper>(
      new Redis::SortedSetWrapper(*this));
}
