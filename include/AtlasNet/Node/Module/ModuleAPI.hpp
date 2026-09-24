#pragma once
#include "AtlasNet/Node/Module/ModuleRegistry.hpp"
#include <cstdint>
namespace AtlasNet::Module
{
inline constexpr uint32_t ABI_VERSION = 1;
inline constexpr const char* ENTRY_POINT = "AtlasNetGetModule";
enum class Capability : uint64_t
{
  None = 0,
  ShardProvider = 1ULL << 0,
  ClientIngressTransport = 1ULL << 1
};
constexpr Capability operator|(Capability left, Capability right) noexcept
{
  return static_cast<Capability>(static_cast<uint64_t>(left) |
                                 static_cast<uint64_t>(right));
}
struct ModuleDescriptor
{
  uint32_t structSize = sizeof(ModuleDescriptor);
  uint32_t abiVersion = ABI_VERSION;
  const char* name = nullptr;
  const char* version = nullptr;
  Capability capabilities = Capability::None;
  void (*Register)(ModuleRegistry&) = nullptr;
};
using GetModuleFunction = const ModuleDescriptor* (*)();
} // namespace AtlasNet::Module
extern "C"
{
  using AtlasNetGetModuleFunction =
      const AtlasNet::Module::ModuleDescriptor* (*)();
}
