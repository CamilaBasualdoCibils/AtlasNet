#pragma once
#include <cstdint>

namespace AtlasNet
{
enum class NodeCapability : uint64_t
{
  None = 0,
  Shard = 1ULL << 0,
  ClientIngress = 1ULL << 1,
  ControllerEligible = 1ULL << 2,
  Database = 1ULL << 3,
};
constexpr NodeCapability operator|(NodeCapability a, NodeCapability b)
{
  return static_cast<NodeCapability>(static_cast<uint64_t>(a) |
                                     static_cast<uint64_t>(b));
}
constexpr bool HasCapability(NodeCapability value, NodeCapability capability)
{
  return (static_cast<uint64_t>(value) & static_cast<uint64_t>(capability)) ==
         static_cast<uint64_t>(capability);
}
inline constexpr auto DefaultNodeCapabilities =
    NodeCapability::Shard | NodeCapability::ClientIngress |
    NodeCapability::ControllerEligible;
} // namespace AtlasNet
