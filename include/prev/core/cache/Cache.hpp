#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
namespace AtlasNet
{
template <typename Key, typename Value> class CacheMap
{
public:
  using DefaultValueProvider = std::function<std::optional<Value>(const Key&)>;
  CacheMap(DefaultValueProvider defaultValueProvider)
      : defaultValueProvider_(std::move(defaultValueProvider))
  {
  }
  std::optional<Value> Get(const Key& key)
  {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end())
    {
      return it->second;
    }
    if (defaultValueProvider_)
    {
      std::optional<Value> v = defaultValueProvider_(key);
      if (!v)
      {
        return std::nullopt;
      }
      lock.unlock();
      std::unique_lock lock(mutex_);
      data_[key] = *v;
      return v;
    }
    return std::nullopt;
  }
  void Invalidate(const Key& key)
  {
    std::unique_lock lock(mutex_);
    data_.erase(key);
  }
  void Clear()
  {
    std::unique_lock lock(mutex_);
    data_.clear();
  }

  std::unique_lock<std::shared_mutex> GetWriteLock()
  {
    return std::unique_lock<std::shared_mutex>(mutex_);
  }
  std::shared_lock<std::shared_mutex> GetReadLock()
  {
    return std::shared_lock<std::shared_mutex>(mutex_);
  }

private:
  std::unordered_map<Key, Value> data_;
  std::shared_mutex mutex_;

  DefaultValueProvider defaultValueProvider_;
};
template <typename First, typename Second> class CacheBiMap
{

public:
  using DefaultFirstProvider =
      std::function<std::optional<First>(const Second&)>;
  using DefaultSecondProvider =
      std::function<std::optional<Second>(const First&)>;
  CacheBiMap(DefaultFirstProvider defaultFirstProvider,
             DefaultSecondProvider defaultSecondProvider)
      : defaultFirstProvider_(std::move(defaultFirstProvider)),
        defaultSecondProvider_(std::move(defaultSecondProvider))
  {
  }
  std::optional<Second> GetByFirst(const First& key)
  {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end())
    {
      return it->second;
    }
    if (defaultSecondProvider_)
    {
      std::optional<Second> v = defaultSecondProvider_(key);
      if (!v)
      {
        return std::nullopt;
      }
      lock.unlock();
      std::unique_lock lock(mutex_);
      data_[key] = *v;
      reverseData_[*v] = key;
      return v;
    }
    return std::nullopt;
  }
  std::optional<First> GetBySecond(const Second& value)
  {
    std::shared_lock lock(mutex_);
    auto it = reverseData_.find(value);
    if (it != reverseData_.end())
    {
      return it->second;
    }
    if (defaultFirstProvider_)
    {
      std::optional<First> k = defaultFirstProvider_(value);
      if (!k)
      {
        return std::nullopt;
      }
      lock.unlock();
      std::unique_lock lock(mutex_);
      data_[*k] = value;
      reverseData_[value] = *k;
      return k;
    }
    return std::nullopt;
  }
  void InvalidateKey(const First& key)
  {
    std::unique_lock lock(mutex_);
    if (auto it = data_.find(key); it != data_.end())
    {
      reverseData_.erase(it->second);
      data_.erase(it);
    }
  }
  void InvalidateValue(const Second& value)
  {
    std::unique_lock lock(mutex_);
    if (auto it = reverseData_.find(value); it != reverseData_.end())
    {
      data_.erase(it->second);
      reverseData_.erase(it);
    }
  }
  void Clear()
  {
    std::unique_lock lock(mutex_);
    data_.clear();
    reverseData_.clear();
  }

  std::unique_lock<std::shared_mutex> GetWriteLock()
  {
    return std::unique_lock<std::shared_mutex>(mutex_);
  }
  std::shared_lock<std::shared_mutex> GetReadLock()
  {
    return std::shared_lock<std::shared_mutex>(mutex_);
  }

private:
  std::unordered_map<First, Second> data_;
  std::unordered_map<Second, First> reverseData_;
  std::shared_mutex mutex_;

  DefaultSecondProvider defaultSecondProvider_;
  DefaultFirstProvider defaultFirstProvider_;
};
} // namespace AtlasNet