#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace AtlasNet
{

inline std::optional<uint64_t> ClaimIDHashTable(Database::RedisConn* _redisConn,
                                                const std::string_view& table,
                                                const std::string_view& value,
                                                uint64_t maxID)
{
  std::string lua = R"(
local table = KEYS[1]
local value = ARGV[1]
local maxID = tonumber(ARGV[2])

for id = 0, maxID do
    if redis.call('HEXISTS', table, tostring(id)) == 0 then
        redis.call('HSET', table, tostring(id), value)
        return id
    end
end

return -1
)";
  const std::string command[] = {"EVAL",
                                 lua,
                                 "1",
                                 (std::string)table,
                                 (std::string)value,
                                 std::to_string(maxID)};

  std::optional<long long> result =
      _redisConn->Command<long long>(std::begin(command), std::end(command));

  if (result == -1)
  {
    /* logger->error("Failed to claim ID for table {} with value {}. Result:
       {}", table, value, result.has_value() ? std::to_string(*result) :
       "none"); */
    return std::nullopt;
  }
  return static_cast<uint64_t>(*result);
}
} // namespace AtlasNet