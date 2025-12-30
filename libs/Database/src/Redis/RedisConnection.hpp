#pragma once
#include <sw/redis++/async_redis++.h>
#include <sw/redis++/redis++.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class RedisConnection
{
   public:
	using StringView = std::string_view;

	template <class T>
	using Future = sw::redis::Future<T>;

	using OptionalString = sw::redis::OptionalString;
	using OptionalDouble = sw::redis::Optional<double>;

	/** Callback invoked with a successfully produced result. */
	template <class T>
	using ResultCb = std::function<void(T)>;

	/** Callback invoked if a Future throws. */
	using ErrorCb = std::function<void(std::exception_ptr)>;

   private:
	bool IsCluster = false;

	// Sync
	std::unique_ptr<sw::redis::Redis> Handle;
	std::unique_ptr<sw::redis::RedisCluster> HandleCluster;

	// Async
	std::unique_ptr<sw::redis::AsyncRedis> HandleAsync;
	std::unique_ptr<sw::redis::AsyncRedisCluster> HandleAsyncCluster;

	template <class F>
	decltype(auto) WithSync(F&& f) const
	{
		if (IsCluster)
			return std::forward<F>(f)(*HandleCluster);
		return std::forward<F>(f)(*Handle);
	}

	template <class F>
	decltype(auto) WithAsync(F&& f) const
	{
		if (IsCluster)
			return std::forward<F>(f)(*HandleAsyncCluster);
		return std::forward<F>(f)(*HandleAsync);
	}

	template <class InT, class Func>
	static auto MapFuture(Future<InT>&& fut, Func&& fn) -> Future<std::invoke_result_t<Func, InT>>
	{
		using OutT = std::invoke_result_t<Func, InT>;
		// std::future has no continuation; we create one via std::async.
		return std::async(std::launch::async,
						  [f = std::move(fut), fn = std::forward<Func>(fn)]() mutable -> OutT
						  { return fn(f.get()); });
	}

	template <class T>
	static void FutureToCallback(Future<T>&& fut, ResultCb<T> ok, ErrorCb err = {})
	{
		std::thread(
			[f = std::move(fut), ok = std::move(ok), err = std::move(err)]() mutable
			{
				try
				{
					ok(f.get());
				}
				catch (...)
				{
					if (err)
						err(std::current_exception());
				}
			})
			.detach();
	}

   public:
	RedisConnection(std::unique_ptr<sw::redis::Redis> redis,
					std::unique_ptr<sw::redis::AsyncRedis> asyncRedis)
		: IsCluster(false), Handle(std::move(redis)), HandleAsync(std::move(asyncRedis))
	{
	}

	RedisConnection(std::unique_ptr<sw::redis::RedisCluster> cluster,
					std::unique_ptr<sw::redis::AsyncRedisCluster> asyncCluster)
		: IsCluster(true),
		  HandleCluster(std::move(cluster)),
		  HandleAsyncCluster(std::move(asyncCluster))
	{
	}

	// -------------------------
	// Example: EXISTS
	// -------------------------
	/**
	 * @brief Count how many of the given key(s) exist.
	 * @details Maps to Redis EXISTS. For a single key, result is 0 or 1.
	 * @note In Cluster, this is a single-key op (safe). Multi-key EXISTS has slot restrictions.
	 */
	[[nodiscard]] long long ExistsCount(StringView key) const;

	/**
	 * @brief Test whether a key exists.
	 * @details Convenience wrapper around ExistsCount(key) != 0.
	 */
	[[nodiscard]] bool Exists(StringView key) const;

	/**
	 * @brief Async count how many of the given key(s) exist.
	 * @return Future resolving to 0 or 1 for a single key.
	 */
	[[nodiscard]] Future<long long> ExistsCountAsync(StringView key) const;

	/**
	 * @brief Async test whether a key exists (Future<bool>).
	 * @details Implemented by mapping Future<long long> → Future<bool>.
	 */
	[[nodiscard]] Future<bool> ExistsAsync(StringView key) const;

	/**
	 * @brief Async test whether a key exists (callback).
	 * @param key Key to check.
	 * @param ok Called with true/false once ready.
	 * @param err Called if the underlying Future throws.
	 */
	void ExistsAsyncCb(StringView key, ResultCb<bool> ok, ErrorCb err = {}) const;

	/**
	 * @brief Delete a key.
	 * @details Maps to Redis DEL. Returns number of keys removed (0 or 1 here).
	 */
	[[nodiscard]] long long Del(StringView key) const;

	/** @brief Async DEL (Future<long long>). */
	[[nodiscard]] Future<long long> DelAsync(StringView key) const;

	/** @brief Async DEL (callback). */
	void DelAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err = {}) const;

	/**
	 * @brief Set a key's time to live in seconds.
	 * @details Maps to Redis EXPIRE.
	 * @return true if the timeout was set; false if key does not exist.
	 */
	[[nodiscard]] bool Expire(StringView key, std::chrono::seconds ttl) const;

	/** @brief Async EXPIRE (Future<bool>). */
	[[nodiscard]] Future<bool> ExpireAsync(StringView key, std::chrono::seconds ttl) const;

	/** @brief Async EXPIRE (callback). */
	void ExpireAsyncCb(StringView key, std::chrono::seconds ttl, ResultCb<bool> ok,
					   ErrorCb err = {}) const;

	/**
	 * @brief Get the remaining time to live of a key (seconds).
	 * @details Maps to Redis TTL. -1 means no expire, -2 means key does not exist.
	 */
	[[nodiscard]] long long TTL(StringView key) const;

	/** @brief Async TTL (Future<long long>). */
	[[nodiscard]] Future<long long> TTLAsync(StringView key) const;

	/** @brief Async TTL (callback). */
	void TTLAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err = {}) const;

	// -------------------------
	// Example: GET / SET
	// -------------------------
	/**
	 * @brief Get the string value of a key.
	 * @details Maps to Redis GET.
	 * @return OptionalString with a value if the key exists, otherwise empty.
	 */
	[[nodiscard]] OptionalString Get(StringView key) const;

	/** @brief Async GET (Future<OptionalString>). */
	[[nodiscard]] Future<OptionalString> GetAsync(StringView key) const;

	/** @brief Async GET (callback). */
	void GetAsyncCb(StringView key, ResultCb<OptionalString> ok, ErrorCb err = {}) const;

	/**
	 * @brief Set the string value of a key.
	 * @details Maps to Redis SET.
	 * @return true on OK.
	 */
	[[nodiscard]] bool Set(StringView key, StringView value) const;

	/** @brief Async SET (Future<bool>). */
	[[nodiscard]] Future<bool> SetAsync(StringView key, StringView value) const;

	/** @brief Async SET (callback). */
	void SetAsyncCb(StringView key, StringView value, ResultCb<bool> ok, ErrorCb err = {}) const;
	/**
	 * @brief Delete the entire hash key (and all its fields) by deleting the key.
	 * @return Number of keys removed: 0 if key didn't exist, 1 if deleted.
	 */
	[[nodiscard]] long long DelKey(const StringView& key) const;
	/**
	 * @brief Async delete of the entire hash key.
	 * @return Future resolving to 0 or 1.
	 */
	[[nodiscard]] sw::redis::Future<long long> DelKeyAsync(const StringView& key) const;
	// -------------------------
	// Callback-style async (optional)
	// -------------------------
	template <typename ReplyT, typename Fn>
	void GetAsyncCb(const StringView& key, Fn&& cb) const
	{
		WithAsyncRedisForKey(StringView{key},
							 [&](auto& r)
							 {
								 r.get(key, std::forward<Fn>(cb));	// cb: void(Future<ReplyT>&&)
							 });
	}

	// =========================================================================
	// Hashes (HSET / HGET / HEXISTS / HDEL / HLEN / HINCRBY / HMGET)
	// =========================================================================

	/**
	 * @brief Set a single field in a hash (HSET).
	 *
	 * Routes by @p key (cluster-safe).
	 *
	 * @param key Hash key.
	 * @param field Field name.
	 * @param value Field value.
	 * @return Number of fields that were newly added (0 if field existed and was overwritten, 1 if
	 * new).
	 */
	[[nodiscard]] long long HSet(const StringView& key, const StringView& field,
								 const StringView& value) const;

	/**
	 * @brief Async HSET (future-based).
	 *
	 * @return Future resolving to number of newly added fields (0 or 1).
	 */
	[[nodiscard]] sw::redis::Future<long long> HSetAsync(const StringView& key,
														 const StringView& field,
														 const StringView& value) const;

	/**
	 * @brief Get a single field from a hash (HGET).
	 *
	 * @return Optional string: empty if the field does not exist.
	 */
	[[nodiscard]] OptionalString HGet(const StringView& key, const StringView& field) const;

	/**
	 * @brief Async HGET (future-based).
	 *
	 * @return Future resolving to OptionalString (empty if field missing).
	 */
	[[nodiscard]] sw::redis::Future<OptionalString> HGetAsync(const StringView& key,
															  const StringView& field) const;
	// Returns all fields + values.
	// NOTE: can be big; consider HSCAN for huge hashes.
	[[nodiscard]] std::unordered_map<std::string, std::string> HGetAll(
		const StringView& key) const;

	[[nodiscard]] sw::redis::Future<std::unordered_map<std::string, std::string>> HGetAllAsync(
		const StringView& key) const;
	/**
	 * @brief Check if a field exists in a hash (HEXISTS).
	 */
	[[nodiscard]] bool HExists(const StringView& key, const StringView& field) const;

	/**
	 * @brief Async HEXISTS (future-based).
	 */
	[[nodiscard]] sw::redis::Future<bool> HExistsAsync(const StringView& key,
													   const StringView& field) const;

	/**
	 * @brief Delete one or more fields from a hash (HDEL).
	 *
	 * @param fields List of fields to delete.
	 * @return Number of fields that were removed.
	 */
	[[nodiscard]] long long HDel(const StringView& key,
								 const std::vector<StringView>& fields) const;

	/**
	 * @brief Async HDEL (future-based).
	 *
	 * @return Future resolving to number of removed fields.
	 */
	[[nodiscard]] sw::redis::Future<long long> HDelAsync(
		const StringView& key, const std::vector<StringView>& fields) const;

	[[nodiscard]] auto HDelAll(const StringView& key) const { return DelKey(key); }
	[[nodiscard]] auto HDelAllAsync(const StringView& key) const { return DelKeyAsync(key); }
	/**
	 * @brief Get number of fields in a hash (HLEN).
	 */
	[[nodiscard]] long long HLen(const StringView& key) const;

	/**
	 * @brief Async HLEN (future-based).
	 */
	[[nodiscard]] sw::redis::Future<long long> HLenAsync(const StringView& key) const;

	/**
	 * @brief Increment an integer field by @p by (HINCRBY).
	 *
	 * Creates the field if it does not exist, treating missing as 0.
	 *
	 * @return The new value after increment.
	 */
	[[nodiscard]] long long HIncrBy(const StringView& key, const StringView& field,
									long long by) const;

	/**
	 * @brief Async HINCRBY (future-based).
	 */
	[[nodiscard]] sw::redis::Future<long long> HIncrByAsync(const StringView& key,
															const StringView& field,
															long long by) const;

	/**
	 * @brief Fetch multiple fields from a hash (HMGET).
	 *
	 * @param fields Fields to fetch.
	 * @return Vector of OptionalString aligned with @p fields order.
	 */
	[[nodiscard]] std::vector<OptionalString> HMGet(const StringView& key,
													const std::vector<StringView>& fields) const;

	/**
	 * @brief Async HMGET (future-based).
	 *
	 * @return Future resolving to vector of OptionalString aligned with @p fields.
	 */
	[[nodiscard]] sw::redis::Future<std::vector<OptionalString>> HMGetAsync(
		const StringView& key, const std::vector<StringView>& fields) const;

	/**
	 * @brief Callback-style async HGET.
	 *
	 * Callback runs on the async event loop thread; keep it fast (enqueue work elsewhere).
	 *
	 * Expected callback signature:
	 *   void(Future<OptionalString>&&)
	 */
	template <typename Fn>
	void HGetAsyncCb(const StringView& key, const StringView& field, Fn&& cb) const
	{
		WithAsyncRedisForKey(StringView{key},
							 [&](auto& r) { r.hget(key, field, std::forward<Fn>(cb)); });
	}
	// =========================================================================
	// Sets (SADD / SREM / SISMEMBER / SCARD / SMEMBERS)
	// =========================================================================

	/**
	 * @brief Add one or more members to a set (SADD).
	 * @return Number of elements actually added (excluding existing ones).
	 */
	[[nodiscard]] long long SAdd(const StringView& key,
								 const std::vector<StringView>& members) const;

	/** @brief Async SADD. */
	[[nodiscard]] sw::redis::Future<long long> SAddAsync(
		const StringView& key, const std::vector<StringView>& members) const;

	/**
	 * @brief Remove one or more members from a set (SREM).
	 * @return Number of elements removed.
	 */
	[[nodiscard]] long long SRem(const StringView& key,
								 const std::vector<StringView>& members) const;

	/** @brief Async SREM. */
	[[nodiscard]] sw::redis::Future<long long> SRemAsync(
		const StringView& key, const std::vector<StringView>& members) const;

	/**
	 * @brief Test if a value is a member of a set (SISMEMBER).
	 */
	[[nodiscard]] bool SIsMember(const StringView& key, const StringView& member) const;

	/** @brief Async SISMEMBER. */
	[[nodiscard]] sw::redis::Future<bool> SIsMemberAsync(const StringView& key,
														 const StringView& member) const;

	/**
	 * @brief Get the number of elements in a set (SCARD).
	 */
	[[nodiscard]] long long SCard(const StringView& key) const;

	/** @brief Async SCARD. */
	[[nodiscard]] sw::redis::Future<long long> SCardAsync(const StringView& key) const;

	/**
	 * @brief Get all members of a set (SMEMBERS).
	 * @note Can be expensive for large sets.
	 */
	[[nodiscard]] std::vector<std::string> SMembers(const StringView& key) const;

	/** @brief Async SMEMBERS. */
	[[nodiscard]] sw::redis::Future<std::vector<std::string>> SMembersAsync(
		const StringView& key) const;

	// =========================================================================
	// Sorted Sets (ZADD / ZREM / ZSCORE / ZRANGE / ZCARD)
	// =========================================================================

	/**
	 * @brief Add or update a member in a sorted set (ZADD).
	 * @return Number of newly added elements.
	 */
	[[nodiscard]] long long ZAdd(const StringView& key, const StringView& member,
								 double score) const;

	/** @brief Async ZADD. */
	[[nodiscard]] sw::redis::Future<long long> ZAddAsync(const StringView& key,
														 const StringView& member,
														 double score) const;

	/**
	 * @brief Remove a member from a sorted set (ZREM).
	 * @return Number of elements removed (0 or 1).
	 */
	[[nodiscard]] long long ZRem(const StringView& key, const StringView& member) const;

	/** @brief Async ZREM. */
	[[nodiscard]] sw::redis::Future<long long> ZRemAsync(const StringView& key,
														 const StringView& member) const;

	/**
	 * @brief Get the score of a member in a sorted set (ZSCORE).
	 */
	[[nodiscard]] OptionalDouble ZScore(const StringView& key, const StringView& member) const;

	/** @brief Async ZSCORE. */
	[[nodiscard]] sw::redis::Future<OptionalDouble> ZScoreAsync(const StringView& key,
																const StringView& member) const;

	/**
	 * @brief Get a range of members by rank (ZRANGE).
	 */
	[[nodiscard]] std::vector<std::string> ZRange(const StringView& key, long long start,
												  long long stop) const;

	/** @brief Async ZRANGE. */
	[[nodiscard]] sw::redis::Future<std::vector<std::string>> ZRangeAsync(const StringView& key,
																		  long long start,
																		  long long stop) const;

	/**
	 * @brief Get number of elements in a sorted set (ZCARD).
	 */
	[[nodiscard]] long long ZCard(const StringView& key) const;

	/** @brief Async ZCARD. */
	[[nodiscard]] sw::redis::Future<long long> ZCardAsync(const StringView& key) const;

	struct RedisTime
	{
		int64_t seconds;
		int64_t microseconds;
	};
	/// @brief Get current Redis server time (authoritative).
	[[nodiscard]] RedisTime GetTimeNow() const;

	/// @brief Convenience helper: seconds since epoch as double.
	[[nodiscard]] double GetTimeNowSeconds() const;
};