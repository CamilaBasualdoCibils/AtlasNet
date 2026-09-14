
#pragma once
#include "atlasnet/core/database/redis/RedisConn.hpp"
#include "sw/redis++/redis.h"
#include <string_view>
namespace AtlasNet::Database::Redis
{

class HashMapWrapper
{
  friend class AtlasNet::Database::RedisConn;
  RedisConn& conn;
  HashMapWrapper(RedisConn& c) : conn(c) {}

  class DeleteWrapper
  {
    friend class HashMapWrapper;
    HashMapWrapper& wrapper;

  protected:
    DeleteWrapper(HashMapWrapper& w) : wrapper(w) {}

  public:
    bool HDel(const std::string_view key, const std::string_view field)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.hdel(key, field) > 0; });
    }

    template <typename T> bool HDel(const std::string_view key, T first, T last)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> bool
          { return handle.hdel(key, first, last) > 0; });
    }

    template <typename T>
    bool HDel(const std::string_view key, std::initializer_list<T> fields)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.hdel(key, fields) > 0; });
    }
  };

  class ExistsWrapper
  {
    friend class HashMapWrapper;
    HashMapWrapper& wrapper;
    ExistsWrapper(HashMapWrapper& w) : wrapper(w) {}

  public:
    bool HExists(const std::string_view key, const std::string_view field)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.hexists(key, field); });
    }
  };

  class GetSetWrapper
  {
    friend class HashMapWrapper;
    HashMapWrapper& wrapper;
    GetSetWrapper(HashMapWrapper& w) : wrapper(w) {}

  public:
    bool HSet(const std::string_view key, const std::string_view field,
              const std::string_view value)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.hset(key, field, value); });
    }

    template <typename Input>
    bool HSet(const std::string_view key, Input first, Input last)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.hset(key, first, last); });
    }

    std::optional<std::string> HGet(const std::string_view key,
                                    const std::string_view field)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> std::optional<std::string>
          { return handle.hget(key, field); });
    }

    template <typename Output>
    void HGetAll(const std::string_view key, Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.hgetall(key, std::forward<Output>(output)); });
    }

    template <typename Input, typename Output>
    void HMGet(const std::string_view key, Input first, Input last,
               Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.hmget(key, first, last, std::forward<Output>(output)); });
    }

    size_t HLen(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.hlen(key); });
    }

    size_t HStrLen(const std::string_view key, const std::string_view field)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.hstrlen(key, field); });
    }

    template <typename Output>
    void HKeys(const std::string_view key, Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.hkeys(key, std::forward<Output>(output)); });
    }

    template <typename Output>
    void HVals(const std::string_view key, Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.hvals(key, std::forward<Output>(output)); });
    }

    long long HIncrBy(const std::string_view key, const std::string_view field,
                      long long increment)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> long long
          { return handle.hincrby(key, field, increment); });
    }

    double HIncrByFloat(const std::string_view key,
                        const std::string_view field, double increment)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> double
          { return handle.hincrbyfloat(key, field, increment); });
    }
  };
  class TTLWrapper
  {
    friend class HashMapWrapper;
    HashMapWrapper& wrapper;
    TTLWrapper(HashMapWrapper& w) : wrapper(w) {}

  public:
    bool HExpire(const std::string_view key, const std::string_view field,
                 long long seconds)
    {
      std::string sec_str = std::to_string(seconds);
      std::string_view command[] = {"HEXPIRE", key, sec_str,
                                    "FIELDS",  "1", field};

      long long resultar[1];
      wrapper.conn.Command(
          std::begin(command), std::end(command), std::begin(resultar));
      return resultar[0] == 1;
    }
  };

public:
  DeleteWrapper Delete()
  {
    return DeleteWrapper(*this);
  }
  ExistsWrapper Exists()
  {
    return ExistsWrapper(*this);
  }
  GetSetWrapper GetSet()
  {
    return GetSetWrapper(*this);
  }
  TTLWrapper TTL()
  {
    return TTLWrapper(*this);
  }
};

} // namespace AtlasNet::Database::Redis