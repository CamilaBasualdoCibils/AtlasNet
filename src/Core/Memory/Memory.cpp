#include "AtlasNet/Core/Memory/Memory.hpp"

#include <mimalloc.h>

#include <new>
#include <stdexcept>

#ifdef ATLASNET_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

namespace AtlasNet::Memory
{
namespace
{
constexpr int TracyCallstackDepth = 16;

[[nodiscard]] void* CheckAllocation(void* ptr, std::size_t size)
{
  if (ptr == nullptr)
    throw std::bad_alloc();
#ifdef ATLASNET_TRACY_ENABLED
  TracyAllocS(ptr, size, TracyCallstackDepth);
#endif
  return ptr;
}
} // namespace

void* Allocate(std::size_t size)
{
  return CheckAllocation(mi_malloc(size), size);
}

void Free(void* ptr) noexcept
{
  if (ptr == nullptr)
    return;
#ifdef ATLASNET_TRACY_ENABLED
  TracyFreeS(ptr, TracyCallstackDepth);
#endif
  mi_free(ptr);
}

void* AllocateAligned(std::size_t size, std::size_t alignment)
{
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    throw std::invalid_argument("alignment must be a non-zero power of two");
  return CheckAllocation(mi_malloc_aligned(size, alignment), size);
}

void FreeAligned(void* ptr) noexcept
{
  Free(ptr);
}
} // namespace AtlasNet::Memory
