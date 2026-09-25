#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <cstdint>

namespace AtlasNet::CmdSig
{

/** Per-target ordering key for atomic, orderable commands/signals. */
struct OrderingKey
{
  AtlasNetEntityID target{};
  uint64_t sequence = 0;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(target, sequence);
  }

  constexpr bool operator==(const OrderingKey&) const = default;
};

inline int CompareOrder(const OrderingKey& a, const OrderingKey& b)
{
  if (a.target.value() < b.target.value())
  {
    return -1;
  }
  if (a.target.value() > b.target.value())
  {
    return 1;
  }
  if (a.sequence < b.sequence)
  {
    return -1;
  }
  if (a.sequence > b.sequence)
  {
    return 1;
  }
  return 0;
}

} // namespace AtlasNet::CmdSig
