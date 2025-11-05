#pragma once
#include <pch.hpp>
#include <future>
#include <vector>
#include <unordered_map>

class IDatabase {
public:
    virtual ~IDatabase() = default;

    /// Initialize connection, halts program until connection established
    virtual bool Connect() = 0;
    /// Initialize connection, returns immidiately
    /// must use .get() if you want to halt program until connection established
    virtual std::future<bool> ConnectAsync() = 0;


    /// Set key -> value. return true if success      
    virtual bool Set(const std::string& key, const std::string& value) = 0;

    // Get value for key (blank if missing)
    virtual std::string Get(const std::string& key) = 0;

    // Remove a data entry
    virtual bool Remove(const std::string& key) = 0;

    /// Check if key exists
    virtual bool Exists(const std::string& key) = 0;

    // hash functions
    /// set key -> field -> value
    /// Ex. HashSet(partition1:foo, bar, baz)
    virtual bool HashSet(const std::string& key, const std::string& field, const std::string& value) = 0;
    /// get value from key -> field 
    virtual std::string HashGet(const std::string& key, const std::string& field) = 0;
    /// get map of all hashes
    virtual std::unordered_map<std::string, std::string> HashGetAll(const std::string& key) = 0;
    /// remove a hash
    virtual bool HashRemove(const std::string& key, const std::string& field) = 0;
    virtual bool HashRemoveAll(const std::string& key) = 0;
    /// check if hash exists
    virtual bool HashExists(const std::string& key, const std::string& field) = 0;
    
    /* START LIST FUNCTIONS */
    // Should generally be using the async version, the non async is just there as an option.
    const static inline int LIST_END = 1;

    virtual std::optional<std::string> ListGetAtIndex(const std::string_view key, long long index) const = 0;
    virtual std::future<std::optional<std::string>> ListGetAtIndexAsync(const std::string_view key, long long index) const = 0;
    // Returns the length of the list
    virtual long long ListInsert(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) = 0;
    virtual std::future<long long> ListInsertAsync(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) = 0;
    virtual long long ListLength(const std::string_view key) const = 0;
    virtual std::future<long long> ListLengthAsync(const std::string_view key) const = 0;
    virtual std::optional<std::string> ListPop(const std::string_view key, const bool front) = 0;
    virtual std::future<std::optional<std::string>> ListPopAsync(const std::string_view key, const bool front) = 0;
    virtual long long ListPush(const std::string_view key, const std::string_view val, const bool front) = 0;
    virtual std::future<long long> ListPushAsync(const std::string_view key, const std::string_view val, const bool front) = 0;
    // Only pushes if the list exists
    virtual long long ListPushx(const std::string_view key, const std::string_view val, const bool front) = 0;
    virtual std::future<long long> ListPushxAsync(const std::string_view key, const std::string_view val, const bool front) = 0;
    // Removes the first 'count' elements that match the 'val' value.
    virtual long long ListRemoveN(const std::string_view key, const long long count, const std::string_view val) = 0;
    virtual std::future<long long> ListRemoveNAsync(const std::string_view key, const long long count, const std::string_view val) = 0;
    virtual void ListSet(const std::string_view key, long long index, const std::string_view val) = 0;
    virtual std::future<void> ListSetAsync(const std::string_view key, long long index, const std::string_view val) = 0;
    virtual void ListTrim(const std::string_view key, long long start, long long stop) = 0;
    virtual std::future<void> ListTrimAsync(const std::string_view key, long long start, long long stop) = 0;
    // pops the last element of 'source' and pushes it to the front of 'destination'
    virtual std::optional<std::string> ListPopPush(const std::string_view source, const std::string_view destination) = 0;
    virtual std::future<std::optional<std::string>> ListPopPushAsync(const std::string_view source, const std::string_view destination) = 0;
    /* END LIST FUNCTIONS */

    /* START SET FUNCTIONS */
    // Adds the member to the set stored at key. Returns number of elements added (0 or 1)
    virtual long long SetAdd(const std::string_view key, const std::string_view member) = 0;
    virtual std::future<long long> SetAddAsync(const std::string_view key, const std::string_view member) = 0;
    virtual long long SetRemove(const std::string_view key, const std::string_view member) = 0;
    virtual std::future<long long> SetRemoveAsync(const std::string_view key, const std::string_view member) = 0;
    virtual bool SetIsMember(const std::string_view key, const std::string_view member) const = 0;
    virtual std::future<bool> SetIsMemberAsync(const std::string_view key, const std::string_view member) const = 0;
    virtual std::vector<std::string> SetMembers(const std::string_view key) const = 0;
    virtual std::future<std::vector<std::string>> SetMembersAsync(const std::string_view key) const = 0;
    /* END SET FUNCTIONS */

    /* START SORTED SET (ZSET) FUNCTIONS */
    // Add member with score to sorted set. Returns number of elements added.
    virtual long long ZAdd(const std::string_view key, const std::string_view member, double score) = 0;
    virtual std::future<long long> ZAddAsync(const std::string_view key, const std::string_view member, double score) = 0;
    virtual long long ZRemove(const std::string_view key, const std::string_view member) = 0;
    virtual std::future<long long> ZRemoveAsync(const std::string_view key, const std::string_view member) = 0;
    virtual std::vector<std::string> ZRangeByScore(const std::string_view key, double minScore, double maxScore, long long limit = -1) const = 0;
    virtual std::future<std::vector<std::string>> ZRangeByScoreAsync(const std::string_view key, double minScore, double maxScore, long long limit = -1) const = 0;
    /* END SORTED SET FUNCTIONS */

    /* START STREAM FUNCTIONS */
    // XADD - adds an entry to a stream, returns entry id
    virtual std::string StreamAdd(const std::string_view key, const std::unordered_map<std::string, std::string> &fields) = 0;
    virtual std::future<std::string> StreamAddAsync(const std::string_view key, const std::unordered_map<std::string, std::string> &fields) = 0;
    /* END STREAM FUNCTIONS */

    /* START GEO FUNCTIONS */
    virtual long long GeoAdd(const std::string_view key, double longitude, double latitude, const std::string_view member) = 0;
    virtual std::future<long long> GeoAddAsync(const std::string_view key, double longitude, double latitude, const std::string_view member) = 0;
    virtual std::vector<std::string> GeoRadius(const std::string_view key, double longitude, double latitude, double radiusMeters, long long count = -1) const = 0;
    virtual std::future<std::vector<std::string>> GeoRadiusAsync(const std::string_view key, double longitude, double latitude, double radiusMeters, long long count = -1) const = 0;
    /* END GEO FUNCTIONS */

    virtual void PrintEntireDB() = 0;
};
