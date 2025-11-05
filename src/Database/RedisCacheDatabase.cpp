#include "RedisCacheDatabase.hpp"

RedisCacheDatabase::RedisCacheDatabase(bool createDatabase, const std::string &host,
                                       int32 port,
                                       const std::string &network)
    : _host(host), _port(port),
      _network(network),
      _autoStart(createDatabase)
{
    if (_autoStart)
    {
        // Ensure data directory exists (mounted volume target)
        std::system("mkdir -p /data");

        // Start Redis process locally (inside same container)
        std::string startCmd =
            "redis-server "
            "--appendonly yes "
            "--appendfilename appendonly.aof "
            "--save 1 1 "
            "--dir /data "
            "--protected-mode no "
            "--daemonize yes";

        int ret = std::system(startCmd.c_str());
        if (ret != 0)
        {
            std::cerr << "❌ Failed to start local Redis process.\n";
            return;
        }

        std::cerr << "🐳 Started Redis (persistent) on host " << _host
                  << ":" << _port << " with /data volume mount.\n";
    }
}

bool RedisCacheDatabase::Connect()
{
    auto future = ConnectAsync();

    // Wait up to 10 seconds (or customize)
    auto status = future.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::ready)
    {
        return future.get();
    }

    std::cerr << "❌ Redis connection timed out.\n";
    return false;
}

std::future<bool> RedisCacheDatabase::ConnectAsync()
{
    using namespace std::chrono_literals;

    return std::async(std::launch::async, [this]() -> bool
                      {
        const int maxAttempts = 20;
        const int delayMs = 500;

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            try {
                // Construct the Redis URI
                std::string uri = "tcp://" + _host + ":" + std::to_string(_port);

                // Try to connect
                _redis = std::make_unique<sw::redis::Redis>(uri);

                // Test the connection
                auto pong = _redis->ping();
                if (pong == "PONG") {
                    std::cerr << "✅ Connected to Redis at " << uri
                              << " on attempt " << attempt << ".\n";
                    return true;
                }

            } catch (const std::exception& e) {
                std::cerr << "⏳ Attempt " << attempt
                          << ": Redis not ready (" << e.what() << ")\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }

        std::cerr << "❌ Failed to connect to Redis after " << maxAttempts << " attempts.\n";
        return false; });
}

bool RedisCacheDatabase::Set(const std::string &key, const std::string &value)
{
    if (!_redis)
        return false;
    return _redis->set(key, value);
}

std::string RedisCacheDatabase::Get(const std::string &key)
{
    if (!_redis)
        return "";

    auto opt = _redis->get(key);
    if (opt)
        return opt.value();
    else
        return "";
}

bool RedisCacheDatabase::Remove(const std::string &key)
{
    // Remove a single key
    if (!_redis)
        return false;
    return _redis->del(key);
}

bool RedisCacheDatabase::Exists(const std::string &key)
{
    if (!_redis)
        return false;
    try {
        return _redis->exists(key) > 0;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis Exists error for key: " << key << " (" << e.what() << ")\n";
        return false;
    }
}

bool RedisCacheDatabase::HashSet(const std::string &key, const std::string &field, const std::string &value)
{
    if (!_redis)
        return false;
    try
    {
        return _redis->hset(key, field, value);
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashSet error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}

std::string RedisCacheDatabase::HashGet(const std::string &key, const std::string &field)
{
    if (!_redis)
        return "";

    try
    {
        auto val = _redis->hget(key, field);
        if (val)
            return *val;
        return "";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashGet error for key: " << key
                  << " field: " << field << " (" << e.what() << ")\n";
        return "";
    }
}

std::unordered_map<std::string, std::string> RedisCacheDatabase::HashGetAll(const std::string &key)
{
    if (!_redis)
        return {};

    try
    {
        std::unordered_map<std::string, std::string> result;
        _redis->hgetall(key, std::inserter(result, result.begin()));
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashGetAll error for key: " << key
                  << " (" << e.what() << ")\n";
        return {};
    }
}

bool RedisCacheDatabase::HashRemove(const std::string &key, const std::string &field)
{
    if (!_redis)
        return false;

    try
    {
        return _redis->hdel(key, field) > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashRemove error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}

bool RedisCacheDatabase::HashRemoveAll(const std::string &key)
{
    if (!_redis)
        return false;

    try
    {
        // Using DEL to remove the entire hash key is faster than iterating over fields
        return _redis->del(key) > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashRemoveAll error for key: " << key
                  << " (" << e.what() << ")\n";
        return false;
    }
}
bool RedisCacheDatabase::HashExists(const std::string &key, const std::string &field)
{
    if (!_redis)
        return false;

    try
    {
        return _redis->hexists(key, field);
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashExists error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}
std::optional<std::string> RedisCacheDatabase::ListGetAtIndex(const std::string_view key, long long index) const {
    if (!_redis) return std::nullopt;
    try {
        auto res = _redis->lindex(key, index);
        return res ? std::make_optional(*res) : std::nullopt;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListGetAtIndex error for key: " << key << " at index: " << index << " (" << e.what() << ")\n";
        return std::nullopt;
    }
}

std::future<std::optional<std::string>> RedisCacheDatabase::ListGetAtIndexAsync(const std::string_view key, long long index) const {
    return std::async(std::launch::async, [this, key, index]() { return this->ListGetAtIndex(key, index); });
}

long long RedisCacheDatabase::ListInsert(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) {
    if (!_redis) return -1;
    try {
        return _redis->linsert(key, before, pivot, val);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListInsert error for key: " << key << " before?: " << before << " pivot point: " << pivot << " when inserting: " << val << " (" << e.what() << ")\n";
        return -1;
    }
}

std::future<long long> RedisCacheDatabase::ListInsertAsync(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) {
    return std::async(std::launch::async, [this, key, before, pivot, val]() { return this->ListInsert(key, before, pivot, val); });
}

long long RedisCacheDatabase::ListLength(const std::string_view key) const {
    if (!_redis) return 0;
    try {
        return _redis->llen(key);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListLength error for key: " << key << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::ListLengthAsync(const std::string_view key) const {
    return std::async(std::launch::async, [this, key]() { return this->ListLength(key); });
}

std::optional<std::string> RedisCacheDatabase::ListPop(const std::string_view &key, const bool front) {
    if (!_redis) return std::nullopt;
    try {
        auto res = (front) ? _redis->lpop(key) : _redis->rpop(key);
        return res ? std::make_optional(*res) : std::nullopt;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListPop error for key: " << key << " front?: " << front << " (" << e.what() << ")\n";
        return std::nullopt;
    }
}

std::future<std::optional<std::string>> RedisCacheDatabase::ListPopAsync(const std::string_view &key, const bool front) {
    return std::async(std::launch::async, [this, key, front]() { return this->ListPop(key, front); });
}

long long RedisCacheDatabase::ListPush(const std::string_view key, const std::string_view val, const bool front) {
    if (!_redis) return -1;
    try {
        return (front) ? _redis->lpush(key, val) : _redis->rpush(key, val);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListPush error for key: " << key << " front?: " << front << " and value: " << val << " (" << e.what() << ")\n";
        return -1;
    }
}

std::future<long long> RedisCacheDatabase::ListPushAsync(const std::string_view key, const std::string_view val, const bool front) {
    return std::async(std::launch::async, [this, key, val, front]() { return this->ListPush(key, val, front); });
}

long long RedisCacheDatabase::ListPushx(const std::string_view key, const std::string_view val, const bool front) {
    if (!_redis) return -1;
    try {
        return (front) ? _redis->lpushx(key, val) : _redis->rpushx(key, val);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListPushx error for key: " << key << " front?: " << front << " and value: " << val << " (" << e.what() << ")\n";
        return -1;
    }
}

std::future<long long> RedisCacheDatabase::ListPushxAsync(const std::string_view key, const std::string_view val, const bool front) {
    return std::async(std::launch::async, [this, key, val, front]() { return this->ListPushx(key, val, front); });
}

long long RedisCacheDatabase::ListRemoveN(const std::string_view key, const long long count, const std::string_view val) {
    if (!_redis) return -1;
    try {
        return _redis->lrem(key, count, val);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListRemoveN error for key: " << key << " count: " << count << " and value: " << val << " (" << e.what() << ")\n";
        return -1;
    }
}

std::future<long long> RedisCacheDatabase::ListRemoveNAsync(const std::string_view key, const long long count, const std::string_view val) {
    return std::async(std::launch::async, [this, key, count, val]() { return this->ListRemoveN(key, count, val); });
}

void RedisCacheDatabase::ListSet(const std::string_view key, long long index, const std::string_view val) {
    if (!_redis) return;
    try {
        _redis->lset(key, index, val);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListSet error for key: " << key << " index: " << index << " and value: " << val << " (" << e.what() << ")\n";
    }
}

std::future<void> RedisCacheDatabase::ListSetAsync(const std::string_view key, long long index, const std::string_view val) {
    return std::async(std::launch::async, [this, key, index, val]() { this->ListSet(key, index, val); });
}

void RedisCacheDatabase::ListTrim(const std::string_view key, long long start, long long stop) {
    if (!_redis) return;
    try {
        _redis->ltrim(key, start, stop);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListTrim error for key: " << key << " start: " << start << " stop: " << stop << " (" << e.what() << ")\n";
    }
}

std::future<void> RedisCacheDatabase::ListTrimAsync(const std::string_view key, long long start, long long stop) {
    return std::async(std::launch::async, [this, key, start, stop]() { this->ListTrim(key, start, stop); });
}

std::optional<std::string> RedisCacheDatabase::ListPopPush(const std::string_view source, const std::string_view destination) {
    if (!_redis) return std::nullopt;
    try {
        auto result = _redis->rpoplpush(source, destination);
        return result ? std::make_optional(*result) : std::nullopt;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ListPopPush error for source: " << source << " destination: " << destination << " (" << e.what() << ")\n";
        return std::nullopt;
    }
}

std::future<std::optional<std::string>> RedisCacheDatabase::ListPopPushAsync(const std::string_view source, const std::string_view destination) {
    return std::async(std::launch::async, [this, source, destination]() { return this->ListPopPush(source, destination); });
}

// -------------------- Set operations --------------------
long long RedisCacheDatabase::SetAdd(const std::string_view key, const std::string_view member) {
    if (!_redis) return 0;
    try {
        return _redis->sadd(key, member);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis SetAdd error for key: " << key << " member: " << member << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::SetAddAsync(const std::string_view key, const std::string_view member) {
    return std::async(std::launch::async, [this, key, member]() { return this->SetAdd(key, member); });
}

long long RedisCacheDatabase::SetRemove(const std::string_view key, const std::string_view member) {
    if (!_redis) return 0;
    try {
        return _redis->srem(key, member);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis SetRemove error for key: " << key << " member: " << member << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::SetRemoveAsync(const std::string_view key, const std::string_view member) {
    return std::async(std::launch::async, [this, key, member]() { return this->SetRemove(key, member); });
}

bool RedisCacheDatabase::SetIsMember(const std::string_view key, const std::string_view member) const {
    if (!_redis) return false;
    try {
        return _redis->sismember(key, member);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis SetIsMember error for key: " << key << " member: " << member << " (" << e.what() << ")\n";
        return false;
    }
}

std::future<bool> RedisCacheDatabase::SetIsMemberAsync(const std::string_view key, const std::string_view member) const {
    return std::async(std::launch::async, [this, key, member]() { return this->SetIsMember(key, member); });
}

std::vector<std::string> RedisCacheDatabase::SetMembers(const std::string_view key) const {
    if (!_redis) return {};
    try {
        std::vector<std::string> members;
        _redis->smembers(key, std::back_inserter(members));
        return members;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis SetMembers error for key: " << key << " (" << e.what() << ")\n";
        return {};
    }
}

std::future<std::vector<std::string>> RedisCacheDatabase::SetMembersAsync(const std::string_view key) const {
    return std::async(std::launch::async, [this, key]() { return this->SetMembers(key); });
}

// -------------------- Sorted set (zset) operations --------------------
long long RedisCacheDatabase::ZAdd(const std::string_view key, const std::string_view member, double score) {
    if (!_redis) return 0;
    try {
        // redis-plus-plus supports zadd; signature may vary by version. This attempts the common overload.
        return _redis->zadd(key, member, score);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ZAdd error for key: " << key << " member: " << member << " score: " << score << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::ZAddAsync(const std::string_view key, const std::string_view member, double score) {
    return std::async(std::launch::async, [this, key, member, score]() { return this->ZAdd(key, member, score); });
}

long long RedisCacheDatabase::ZRemove(const std::string_view key, const std::string_view member) {
    if (!_redis) return 0;
    try {
        return _redis->zrem(key, member);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ZRemove error for key: " << key << " member: " << member << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::ZRemoveAsync(const std::string_view key, const std::string_view member) {
    return std::async(std::launch::async, [this, key, member]() { return this->ZRemove(key, member); });
}

std::vector<std::string> RedisCacheDatabase::ZRangeByScore(const std::string_view key, double minScore, double maxScore, long long limit) const {
    if (!_redis) return {};
    try {
        std::vector<std::string> items;
        if (limit > 0) {
            _redis->zrangebyscore(key, std::to_string(minScore), std::to_string(maxScore), 0, limit, std::back_inserter(items));
        } else {
            _redis->zrangebyscore(key, std::to_string(minScore), std::to_string(maxScore), std::back_inserter(items));
        }
        return items;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis ZRangeByScore error for key: " << key << " (" << e.what() << ")\n";
        return {};
    }
}

std::future<std::vector<std::string>> RedisCacheDatabase::ZRangeByScoreAsync(const std::string_view key, double minScore, double maxScore, long long limit) const {
    return std::async(std::launch::async, [this, key, minScore, maxScore, limit]() { return this->ZRangeByScore(key, minScore, maxScore, limit); });
}

// -------------------- Stream operations --------------------
std::string RedisCacheDatabase::StreamAdd(const std::string_view key, const std::unordered_map<std::string, std::string> &fields) {
    if (!_redis) return {};
    try {
        // xadd with auto id
        return _redis->xadd(key, "*", fields.begin(), fields.end());
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis StreamAdd error for key: " << key << " (" << e.what() << ")\n";
        return {};
    }
}

std::future<std::string> RedisCacheDatabase::StreamAddAsync(const std::string_view key, const std::unordered_map<std::string, std::string> &fields) {
    return std::async(std::launch::async, [this, key, &fields]() { return this->StreamAdd(key, fields); });
}

// -------------------- Geo operations --------------------
long long RedisCacheDatabase::GeoAdd(const std::string_view key, double longitude, double latitude, const std::string_view member) {
    if (!_redis) return 0;
    try {
        return _redis->geoadd(key, longitude, latitude, member);
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis GeoAdd error for key: " << key << " member: " << member << " (" << e.what() << ")\n";
        return 0;
    }
}

std::future<long long> RedisCacheDatabase::GeoAddAsync(const std::string_view key, double longitude, double latitude, const std::string_view member) {
    return std::async(std::launch::async, [this, key, longitude, latitude, member]() { return this->GeoAdd(key, longitude, latitude, member); });
}

std::vector<std::string> RedisCacheDatabase::GeoRadius(const std::string_view key, double longitude, double latitude, double radiusMeters, long long count) const {
    if (!_redis) return {};
    try {
        std::vector<std::string> members;
        // use meters as unit
        if (count > 0) {
            _redis->georadius(key, longitude, latitude, radiusMeters, sw::redis::GeoUnit::kMeters, 0, count, std::back_inserter(members));
        } else {
            _redis->georadius(key, longitude, latitude, radiusMeters, sw::redis::GeoUnit::kMeters, std::back_inserter(members));
        }
        return members;
    } catch (const std::exception &e) {
        std::cerr << "⚠️ Redis GeoRadius error for key: " << key << " (" << e.what() << ")\n";
        return {};
    }
}

std::future<std::vector<std::string>> RedisCacheDatabase::GeoRadiusAsync(const std::string_view key, double longitude, double latitude, double radiusMeters, long long count) const {
    return std::async(std::launch::async, [this, key, longitude, latitude, radiusMeters, count]() { return this->GeoRadius(key, longitude, latitude, radiusMeters, count); });
}


void RedisCacheDatabase::PrintEntireDB()
{
    if (!_redis)
    {
        std::cerr << "❌ No Redis connection.\n";
        return;
    }

    long long cursor = 0;
    do
    {
        std::vector<std::string> keys;
        cursor = _redis->scan(cursor, "*", 100, std::back_inserter(keys));

        for (const auto &key : keys)
        {
            try
            {
                // Detect type
                const auto type = _redis->type(key);

                if (type == "string")
                {
                    const auto val = _redis->get(key);
                    std::cerr << key << " (string) = " << (val ? *val : "(nil)") << "\n";
                }
                else if (type == "hash")
                {
                    std::unordered_map<std::string, std::string> fields;
                    _redis->hgetall(key, std::inserter(fields, fields.begin()));
                    std::cerr << key << " (hash):\n";
                    for (const auto &kv : fields)
                    {
                        std::cerr << "    " << kv.first << " = " << kv.second << "\n";
                    }
                }
                else if (type == "set")
                {
                    std::vector<std::string> members;
                    _redis->smembers(key, std::back_inserter(members));
                    std::cerr << key << " (set): { ";
                    for (const auto &m : members)
                        std::cerr << m << " ";
                    std::cerr << "}\n";
                }
                else if (type == "list")
                {
                    std::vector<std::string> items;
                    _redis->lrange(key, 0, -1, std::back_inserter(items));
                    std::cerr << key << " (list): [ ";
                    for (const auto &i : items)
                        std::cerr << i << " ";
                    std::cerr << "]\n";
                }
                else if (type == "zset")
                {
                    std::vector<std::string> members;
                    _redis->zmembers(key, std::back_inserter(members));
                    std::cerr << key << " (set): { ";
                    for (const auto& m: members){
                        std::cerr << m << " ";
                    }
                    std::cerr << "}\n";
                }
                else if (type == "stream")
                {
                    const auto entries = _redis->xrange(key, "-", "+");
                    std::cerr << key << " (stream):\n";
                    for (const auto& entry : entries) {
                        std::cerr << "    ID: " << entry.first << "\n";
                        std::cerr << "    Fields:\n";
                        for (const auto& field : entry.second) {
                            std::cerr << "        " << field.first << ": " << field.second << "\n";
                        }
                    }
                }
                else if (type == "geo")
                {
                    std::vector<std::string> members;
                    _redis->zrange(key, 0, -1, std::back_inserter(members));
                    
                    std::cerr << key << " (geo):\n";
                    for (const auto& member : members) {
                        // Get position for each member
                        auto pos = _redis->geopos(key, member);
                        if (pos && !pos->empty() && (*pos)[0]) {
                            const auto& coords = *(*pos)[0];
                            std::cerr << "    " << member << ": "
                                    << "longitude=" << coords.longitude << ", "
                                    << "latitude=" << coords.latitude << "\n";
                        }
                    }
                }
                else
                {
                    std::cerr << key << " (unknown type: " << type << ")\n";
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "⚠️ Error reading key " << key << ": " << e.what() << "\n";
            }
        }
    } while (cursor != 0);
}