#pragma once

#include "AtlasNet/Core/SpatialObject/ISpatialObjectIndex.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace AtlasNet::SpatialObject
{

/**
 * Lock-free open-addressing ID → slot map with inline published POD + seqlock.
 * Remove of slot A does not block insert of distinct IDs (independent CAS).
 * Tombstones allow reuse; generation prevents ABA on handles.
 */
class LockFreeSpatialObjectIndex final : public ISpatialObjectIndex
{
public:
  explicit LockFreeSpatialObjectIndex(std::size_t capacity = 1024)
      : capacity_(NextPowerOfTwo(capacity < 16 ? 16 : capacity)),
        mask_(capacity_ - 1), slots_(std::make_unique<Slot[]>(capacity_)),
        size_(0)
  {
  }

  InsertResult Insert(const AtlasNetEntityID& id,
                      const PublishedState& initial) override
  {
    const uint64_t key = id.value();
    for (std::size_t probe = 0; probe < capacity_; ++probe)
    {
      const std::size_t idx = (Hash(key) + probe) & mask_;
      Slot& slot = slots_[idx];
      uint64_t state = slot.state.load(std::memory_order_acquire);

      for (;;)
      {
        const uint32_t gen = StateGeneration(state);
        if (IsOccupied(state))
        {
          if (slot.idValue.load(std::memory_order_relaxed) == key)
          {
            return InsertResult::AlreadyExists;
          }
          break; // try next probe
        }

        // Empty or tombstone: try claim
        const uint32_t newGen = gen == 0 ? 1 : gen + (IsTombstone(state) ? 1 : 0);
        const uint64_t desired = MakeState(newGen, OccupiedBit);
        if (slot.state.compare_exchange_weak(state, desired,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
        {
          slot.idValue.store(key, std::memory_order_relaxed);
          WritePublishedUnlocked(slot, initial);
          size_.fetch_add(1, std::memory_order_relaxed);
          return InsertResult::Ok;
        }
        // CAS failed; re-read state and retry same slot or continue
        if (IsOccupied(state) &&
            slot.idValue.load(std::memory_order_relaxed) == key)
        {
          return InsertResult::AlreadyExists;
        }
        if (IsOccupied(state))
        {
          break;
        }
      }
    }
    return InsertResult::TableFull;
  }

  RemoveResult Remove(const AtlasNetEntityID& id) override
  {
    auto handle = Find(id);
    if (!handle)
    {
      return RemoveResult::NotFound;
    }
    return Remove(*handle);
  }

  RemoveResult Remove(SpatialObjectHandle handle) override
  {
    if (handle.slot >= capacity_)
    {
      return RemoveResult::NotFound;
    }
    Slot& slot = slots_[handle.slot];
    uint64_t state = slot.state.load(std::memory_order_acquire);
    for (;;)
    {
      if (!IsOccupied(state) || StateGeneration(state) != handle.generation)
      {
        return RemoveResult::StaleHandle;
      }
      const uint64_t tomb =
          MakeState(handle.generation, TombstoneBit);
      if (slot.state.compare_exchange_weak(state, tomb, std::memory_order_acq_rel,
                                          std::memory_order_acquire))
      {
        size_.fetch_sub(1, std::memory_order_relaxed);
        return RemoveResult::Ok;
      }
    }
  }

  [[nodiscard]] std::optional<SpatialObjectHandle>
  Find(const AtlasNetEntityID& id) const override
  {
    const uint64_t key = id.value();
    for (std::size_t probe = 0; probe < capacity_; ++probe)
    {
      const std::size_t idx = (Hash(key) + probe) & mask_;
      const Slot& slot = slots_[idx];
      const uint64_t state = slot.state.load(std::memory_order_acquire);
      if (IsEmpty(state))
      {
        return std::nullopt;
      }
      if (IsOccupied(state) &&
          slot.idValue.load(std::memory_order_relaxed) == key)
      {
        return SpatialObjectHandle{static_cast<uint32_t>(idx),
                                   StateGeneration(state)};
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] bool Contains(const AtlasNetEntityID& id) const override
  {
    return Find(id).has_value();
  }

  [[nodiscard]] std::optional<PublishedState>
  ReadPublished(const AtlasNetEntityID& id) const override
  {
    auto handle = Find(id);
    if (!handle)
    {
      return std::nullopt;
    }
    return ReadPublished(*handle);
  }

  [[nodiscard]] std::optional<PublishedState>
  ReadPublished(SpatialObjectHandle handle) const override
  {
    if (handle.slot >= capacity_)
    {
      return std::nullopt;
    }
    const Slot& slot = slots_[handle.slot];
    for (;;)
    {
      const uint64_t state = slot.state.load(std::memory_order_acquire);
      if (!IsOccupied(state) || StateGeneration(state) != handle.generation)
      {
        return std::nullopt;
      }
      uint64_t v1 = slot.version.load(std::memory_order_acquire);
      if (v1 & 1ULL)
      {
        continue; // write in progress
      }
      PublishedState copy = slot.published;
      std::atomic_thread_fence(std::memory_order_acquire);
      uint64_t v2 = slot.version.load(std::memory_order_acquire);
      if (v1 == v2)
      {
        const uint64_t state2 = slot.state.load(std::memory_order_acquire);
        if (!IsOccupied(state2) ||
            StateGeneration(state2) != handle.generation)
        {
          return std::nullopt;
        }
        return copy;
      }
    }
  }

  bool Publish(const AtlasNetEntityID& id, const PublishedState& state) override
  {
    auto handle = Find(id);
    if (!handle)
    {
      return false;
    }
    Slot& slot = slots_[handle->slot];
    const uint64_t st = slot.state.load(std::memory_order_acquire);
    if (!IsOccupied(st) || StateGeneration(st) != handle->generation)
    {
      return false;
    }
    WritePublishedUnlocked(slot, state);
    return true;
  }

  [[nodiscard]] std::size_t Size() const override
  {
    return size_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::size_t Capacity() const override
  {
    return capacity_;
  }

private:
  static constexpr uint64_t OccupiedBit = 1ULL << 63;
  static constexpr uint64_t TombstoneBit = 1ULL << 62;
  static constexpr uint64_t GenerationMask = (1ULL << 32) - 1;

  struct Slot
  {
    std::atomic<uint64_t> state{0};
    std::atomic<uint64_t> idValue{0};
    // seqlock: odd = write in progress
    mutable std::atomic<uint64_t> version{0};
    PublishedState published{};
  };

  static uint64_t MakeState(uint32_t generation, uint64_t flags)
  {
    return (static_cast<uint64_t>(generation) & GenerationMask) | flags;
  }
  static uint32_t StateGeneration(uint64_t state)
  {
    return static_cast<uint32_t>(state & GenerationMask);
  }
  static bool IsOccupied(uint64_t state)
  {
    return (state & OccupiedBit) != 0;
  }
  static bool IsTombstone(uint64_t state)
  {
    return (state & TombstoneBit) != 0 && !IsOccupied(state);
  }
  static bool IsEmpty(uint64_t state)
  {
    return state == 0;
  }

  static std::size_t NextPowerOfTwo(std::size_t v)
  {
    std::size_t p = 1;
    while (p < v)
    {
      p <<= 1;
    }
    return p;
  }

  static std::size_t Hash(uint64_t key)
  {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return static_cast<std::size_t>(key);
  }

  static void WritePublishedUnlocked(Slot& slot, const PublishedState& state)
  {
    uint64_t v = slot.version.load(std::memory_order_relaxed);
    slot.version.store(v + 1, std::memory_order_release); // odd
    slot.published = state;
    slot.version.store(v + 2, std::memory_order_release); // even
  }

  std::size_t capacity_;
  std::size_t mask_;
  std::unique_ptr<Slot[]> slots_;
  std::atomic<std::size_t> size_;
};

} // namespace AtlasNet::SpatialObject
