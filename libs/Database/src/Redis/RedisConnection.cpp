#include "RedisConnection.hpp"

#include <hiredis/read.h>

#include <iostream>
#include <string_view>

long long RedisConnection::HSet(const StringView& key, const StringView& field,
								const StringView& value) const
{
	return WithSync([&](auto& r) -> long long { return r.hset(key, field, value); });
}

sw::redis::Future<long long> RedisConnection::HSetAsync(const StringView& key,
														const StringView& field,
														const StringView& value) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long>
					 { return r.hset(key, field, value); });
}

RedisConnection::OptionalString RedisConnection::HGet(const StringView& key,
													  const StringView& field) const
{
	return WithSync([&](auto& r) -> OptionalString { return r.hget(key, field); });
}

sw::redis::Future<RedisConnection::OptionalString> RedisConnection::HGetAsync(
	const StringView& key, const StringView& field) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<OptionalString>
					 { return r.hget(key, field); });
}

bool RedisConnection::HExists(const StringView& key, const StringView& field) const
{
	return WithSync([&](auto& r) -> bool { return r.hexists(key, field); });
}

sw::redis::Future<bool> RedisConnection::HExistsAsync(const StringView& key,
													  const StringView& field) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<bool> { return r.hexists(key, field); });
}

long long RedisConnection::HDel(const StringView& key,
								const std::vector<StringView>& fields) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (fields.empty())
				return 0LL;
			return r.hdel(key, fields.begin(), fields.end());
		});
}

sw::redis::Future<long long> RedisConnection::HDelAsync(
	const StringView& key, const std::vector<StringView>& fields) const
{
	return WithAsync(

		[&](auto& r) -> sw::redis::Future<long long>
		{
			if (fields.empty())
				return r.template command<long long>(
					"ECHO", "0");  // cheap resolved future alternative is library-dependent
			return r.hdel(key, fields.begin(), fields.end());
		});
}

long long RedisConnection::HLen(const StringView& key) const
{
	return WithSync([&](auto& r) -> long long { return r.hlen(key); });
}

sw::redis::Future<long long> RedisConnection::HLenAsync(const StringView& key) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long> { return r.hlen(key); });
}

long long RedisConnection::HIncrBy(const StringView& key, const StringView& field,
								   long long by) const
{
	return WithSync([&](auto& r) -> long long { return r.hincrby(key, field, by); });
}

sw::redis::Future<long long> RedisConnection::HIncrByAsync(const StringView& key,
														   const StringView& field,
														   long long by) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long>
					 { return r.hincrby(key, field, by); });
}

std::vector<RedisConnection::OptionalString> RedisConnection::HMGet(
	const StringView& key, const std::vector<StringView>& fields) const
{
	return WithSync(
		[&](auto& r) -> std::vector<OptionalString>
		{
			std::vector<OptionalString> out;
			out.reserve(fields.size());
			if (!fields.empty())
			{
				r.hmget(key, fields.begin(), fields.end(), std::back_inserter(out));
			}
			return out;
		});
}

sw::redis::Future<std::vector<RedisConnection::OptionalString>> RedisConnection::HMGetAsync(
	const StringView& key, const std::vector<StringView>& fields) const
{
	return WithAsync(
		[&](auto& r) -> sw::redis::Future<std::vector<OptionalString>>
		{
			// Many redis++ async methods can fill an output iterator (like
			// sync). If your async version doesn't support OutputIt, tell me
			// your redis++ version and I'll adapt this to a command()-based
			// parsing approach.
			std::vector<OptionalString> out;
			out.reserve(fields.size());

			// If async hmget(OutputIt) exists:
			return r.template hmget<std::vector<OptionalString>>(key, fields.begin(), fields.end());
		});
}
std::unordered_map<std::string, std::string> RedisConnection::HGetAll(const StringView& key) const
{
	std::unordered_map<std::string, std::string> out;

	WithSync(
		[&](auto& r)
		{
			// redis++ fills an output iterator of pairs
			r.hgetall(key, std::inserter(out, out.end()));
		});

	return out;
}
sw::redis::Future<std::unordered_map<std::string, std::string>> RedisConnection::HGetAllAsync(
	const StringView& key) const
{
	return WithAsync(

		[&](auto& r) -> sw::redis::Future<std::unordered_map<std::string, std::string>>
		{ return r.template hgetall<std::unordered_map<std::string, std::string>>(key); });
}
long long RedisConnection::DelKey(const StringView& key) const
{
	return WithSync([&](auto& r) -> long long { return r.del(key); });
}
sw::redis::Future<long long> RedisConnection::DelKeyAsync(const StringView& key) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long> { return r.del(key); });
}
RedisConnection::OptionalString RedisConnection::Get(StringView key) const
{
	return WithSync([&](auto& r) { return r.get(key); });
}
RedisConnection::Future<RedisConnection::OptionalString> RedisConnection::GetAsync(
	StringView key) const
{
	return WithAsync([&](auto& r) { return r.get(key); });
}
void RedisConnection::GetAsyncCb(StringView key, ResultCb<OptionalString> ok, ErrorCb err) const
{
	FutureToCallback<OptionalString>(GetAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Set(StringView key, StringView value) const
{
	return WithSync([&](auto& r) { return r.set(key, value); });
}
RedisConnection::Future<bool> RedisConnection::SetAsync(StringView key, StringView value) const
{
	return WithAsync([&](auto& r) { return r.set(key, value); });
}
void RedisConnection::SetAsyncCb(StringView key, StringView value, ResultCb<bool> ok,
								 ErrorCb err) const
{
	FutureToCallback<bool>(SetAsync(key, value), std::move(ok), std::move(err));
}
long long RedisConnection::ExistsCount(StringView key) const
{
	return WithSync([&](auto& r) { return r.exists(key); });
}
bool RedisConnection::Exists(StringView key) const
{
	return ExistsCount(key) != 0;
}
RedisConnection::Future<long long> RedisConnection::ExistsCountAsync(StringView key) const
{
	return WithAsync([&](auto& r) { return r.exists(key); });
}
RedisConnection::Future<bool> RedisConnection::ExistsAsync(StringView key) const
{
	return MapFuture<long long>(ExistsCountAsync(key), [](long long n) { return n != 0; });
}
void RedisConnection::ExistsAsyncCb(StringView key, ResultCb<bool> ok, ErrorCb err) const
{
	FutureToCallback<bool>(ExistsAsync(key), std::move(ok), std::move(err));
}
long long RedisConnection::Del(StringView key) const
{
	return WithSync([&](auto& r) { return r.del(key); });
}
RedisConnection::Future<long long> RedisConnection::DelAsync(StringView key) const
{
	return WithAsync([&](auto& r) { return r.del(key); });
}
void RedisConnection::DelAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(DelAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Expire(StringView key, std::chrono::seconds ttl) const
{
	return WithSync([&](auto& r) { return r.expire(key, ttl); });
}
RedisConnection::Future<bool> RedisConnection::ExpireAsync(StringView key,
														   std::chrono::seconds ttl) const
{
	return WithAsync([&](auto& r) { return r.expire(key, ttl); });
}
void RedisConnection::ExpireAsyncCb(StringView key, std::chrono::seconds ttl, ResultCb<bool> ok,
									ErrorCb err) const
{
	FutureToCallback<bool>(ExpireAsync(key, ttl), std::move(ok), std::move(err));
}
long long RedisConnection::TTL(StringView key) const
{
	return WithSync([&](auto& r) { return r.ttl(key); });
}
RedisConnection::Future<long long> RedisConnection::TTLAsync(StringView key) const
{
	return WithAsync([&](auto& r) { return r.ttl(key); });
}
void RedisConnection::TTLAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(TTLAsync(key), std::move(ok), std::move(err));
}

// =====================
// Sets
// =====================

long long RedisConnection::SAdd(const StringView& key,
								const std::vector<StringView>& members) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (members.empty())
				return 0LL;
			return r.sadd(key, members.begin(), members.end());
		});
}

sw::redis::Future<long long> RedisConnection::SAddAsync(
	const StringView& key, const std::vector<StringView>& members) const
{
	return WithAsync(
		[&](auto& r) -> sw::redis::Future<long long>
		{
			if (members.empty())
				return r.template command<long long>("ECHO", "0");
			return r.sadd(key, members.begin(), members.end());
		});
}

long long RedisConnection::SRem(const StringView& key,
								const std::vector<StringView>& members) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (members.empty())
				return 0LL;
			return r.srem(key, members.begin(), members.end());
		});
}

sw::redis::Future<long long> RedisConnection::SRemAsync(
	const StringView& key, const std::vector<StringView>& members) const
{
	return WithAsync(
		[&](auto& r) -> sw::redis::Future<long long>
		{
			if (members.empty())
				return r.template command<long long>("ECHO", "0");
			return r.srem(key, members.begin(), members.end());
		});
}

bool RedisConnection::SIsMember(const StringView& key, const StringView& member) const
{
	return WithSync([&](auto& r) -> bool { return r.sismember(key, member); });
}

sw::redis::Future<bool> RedisConnection::SIsMemberAsync(const StringView& key,
														const StringView& member) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<bool> { return r.sismember(key, member); });
}

long long RedisConnection::SCard(const StringView& key) const
{
	return WithSync([&](auto& r) -> long long { return r.scard(key); });
}

sw::redis::Future<long long> RedisConnection::SCardAsync(const StringView& key) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long> { return r.scard(key); });
}

std::vector<std::string> RedisConnection::SMembers(const StringView& key) const
{
	return WithSync(
		[&](auto& r) -> std::vector<std::string>
		{
			std::vector<std::string> out;
			r.smembers(key, std::back_inserter(out));
			return out;
		});
}

sw::redis::Future<std::vector<std::string>> RedisConnection::SMembersAsync(
	const StringView& key) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<std::vector<std::string>>
					 { return r.template smembers<std::vector<std::string>>(key); });
}

// =====================
// Sorted Sets
// =====================

long long RedisConnection::ZAdd(const StringView& key, const StringView& member,
								double score) const
{
	return WithSync([&](auto& r) -> long long { return r.zadd(key, member, score); });
}

sw::redis::Future<long long> RedisConnection::ZAddAsync(const StringView& key,
														const StringView& member,
														double score) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long>
					 { return r.zadd(key, member, score); });
}

long long RedisConnection::ZRem(const StringView& key, const StringView& member) const
{
	return WithSync([&](auto& r) -> long long { return r.zrem(key, member); });
}

sw::redis::Future<long long> RedisConnection::ZRemAsync(const StringView& key,
														const StringView& member) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long> { return r.zrem(key, member); });
}

RedisConnection::OptionalDouble RedisConnection::ZScore(const StringView& key,
														const StringView& member) const
{
	return WithSync([&](auto& r) -> OptionalDouble { return r.zscore(key, member); });
}

sw::redis::Future<RedisConnection::OptionalDouble> RedisConnection::ZScoreAsync(
	const StringView& key, const StringView& member) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<OptionalDouble>
					 { return r.zscore(key, member); });
}

std::vector<std::string> RedisConnection::ZRange(const StringView& key, long long start,
												 long long stop) const
{
	return WithSync(
		[&](auto& r) -> std::vector<std::string>
		{
			std::vector<std::string> out;
			r.zrange(key, start, stop, std::back_inserter(out));
			return out;
		});
}

sw::redis::Future<std::vector<std::string>> RedisConnection::ZRangeAsync(const StringView& key,
																		 long long start,
																		 long long stop) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<std::vector<std::string>>
					 { return r.template zrange<std::vector<std::string>>(key, start, stop); });
}

long long RedisConnection::ZCard(const StringView& key) const
{
	return WithSync([&](auto& r) -> long long { return r.zcard(key); });
}

sw::redis::Future<long long> RedisConnection::ZCardAsync(const StringView& key) const
{
	return WithAsync([&](auto& r) -> sw::redis::Future<long long> { return r.zcard(key); });
}
RedisConnection::RedisTime RedisConnection::GetTimeNow() const
{
	// Redis TIME command returns: [seconds, microseconds]
	const std::array<StringView, 1> args = {"TIME"};
	try
	{
		auto res = WithSync([&](auto& r) -> auto { return r.command(args.begin(), args.end()); });
		if (res->type != REDIS_REPLY_ARRAY)
		{
			std::cerr << "GetTimeNow Unknown reply of type " << res->type << std::endl;
			return RedisTime{};
		}
		const redisReply* secReply = res->element[0];
		const redisReply* usecReply = res->element[1];
		int64_t seconds = 0;
		std::from_chars(secReply->str, secReply->str + secReply->len, seconds);

		int64_t microseconds = 0;
		std::from_chars(usecReply->str, usecReply->str + usecReply->len, microseconds);

		return RedisTime{seconds, microseconds};
	}
	catch (...)
	{
		return RedisTime{0, 0};
	}
}
double RedisConnection::GetTimeNowSeconds() const
{
	const auto t = GetTimeNow();
	return static_cast<double>(t.seconds) + static_cast<double>(t.microseconds) * 1e-6;
}
