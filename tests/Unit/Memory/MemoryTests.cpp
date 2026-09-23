#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AtlasNet/Core/Memory/Memory.hpp"

namespace
{
struct TrackedObject
{
  TrackedObject(int value, std::atomic<int>& constructions,
                std::atomic<int>& destructions)
      : Value(value), Destructions(destructions)
  {
    ++constructions;
  }
  ~TrackedObject()
  {
    ++Destructions;
  }
  int Value;
  std::atomic<int>& Destructions;
};
} // namespace

TEST(Memory, AllocateAndFree)
{
  void* ptr = AtlasNet::Memory::Allocate(128);
  ASSERT_NE(ptr, nullptr);
  AtlasNet::Memory::Free(ptr);
}

TEST(Memory, ZeroAndSmallAllocations)
{
  void* zero = AtlasNet::Memory::Allocate(0);
  void* small = AtlasNet::Memory::Allocate(1);
  EXPECT_NE(zero, nullptr);
  EXPECT_NE(small, nullptr);
  AtlasNet::Memory::Free(zero);
  AtlasNet::Memory::Free(small);
  AtlasNet::Memory::Free(nullptr);
}

TEST(Memory, LargeAllocation)
{
  constexpr std::size_t Size = 16 * 1024 * 1024;
  auto* ptr = static_cast<std::byte*>(AtlasNet::Memory::Allocate(Size));
  ASSERT_NE(ptr, nullptr);
  ptr[0] = std::byte{0x12};
  ptr[Size - 1] = std::byte{0x34};
  AtlasNet::Memory::Free(ptr);
}

TEST(Memory, AlignedAllocation)
{
  constexpr std::size_t Alignment = 256;
  void* ptr = AtlasNet::Memory::AllocateAligned(1024, Alignment);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ptr) % Alignment, 0U);
  AtlasNet::Memory::FreeAligned(ptr);
}

TEST(Memory, RejectsInvalidAlignment)
{
  EXPECT_THROW((void)AtlasNet::Memory::AllocateAligned(32, 3),
               std::invalid_argument);
}

TEST(Memory, MultipleAllocations)
{
  std::vector<void*> allocations;
  for (std::size_t i = 1; i <= 1024; ++i)
    allocations.push_back(AtlasNet::Memory::Allocate(i));
  for (void* ptr : allocations)
    AtlasNet::Memory::Free(ptr);
}

TEST(Memory, NewAndDeleteRunConstructorAndDestructor)
{
  std::atomic<int> constructions = 0;
  std::atomic<int> destructions = 0;
  auto* object =
      AtlasNet::Memory::New<TrackedObject>(42, constructions, destructions);
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(object->Value, 42);
  EXPECT_EQ(constructions.load(), 1);
  EXPECT_EQ(destructions.load(), 0);
  AtlasNet::Memory::Delete(object);
  EXPECT_EQ(destructions.load(), 1);
  AtlasNet::Memory::Delete<TrackedObject>(nullptr);
}

TEST(Memory, VectorAllocator)
{
  std::vector<int, AtlasNet::Memory::Allocator<int>> values;
  for (int i = 0; i < 1000; ++i)
    values.push_back(i);
  ASSERT_EQ(values.size(), 1000U);
  EXPECT_EQ(values.front(), 0);
  EXPECT_EQ(values.back(), 999);
}

TEST(Memory, NodeBasedContainerAllocator)
{
  using Entry = std::pair<const int, int>;
  std::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
                     AtlasNet::Memory::Allocator<Entry>>
      values;
  values.emplace(1, 10);
  values.emplace(2, 20);
  EXPECT_EQ(values.at(1), 10);
  EXPECT_EQ(values.at(2), 20);
}

TEST(Memory, MultipleThreadsAllocateAndFree)
{
  constexpr int ThreadCount = 8;
  constexpr int AllocationCount = 1000;
  std::vector<std::thread> threads;
  threads.reserve(ThreadCount);
  for (int thread = 0; thread < ThreadCount; ++thread)
  {
    threads.emplace_back(
        []
        {
          for (int allocation = 0; allocation < AllocationCount; ++allocation)
          {
            void* ptr = AtlasNet::Memory::Allocate(64);
            AtlasNet::Memory::Free(ptr);
          }
        });
  }
  for (auto& thread : threads)
    thread.join();
}

TEST(Memory, AllocateOnOneThreadAndFreeOnAnother)
{
  void* ptr = nullptr;
  std::thread allocator([&ptr] { ptr = AtlasNet::Memory::Allocate(4096); });
  allocator.join();
  ASSERT_NE(ptr, nullptr);
  std::thread deallocator([ptr] { AtlasNet::Memory::Free(ptr); });
  deallocator.join();
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
