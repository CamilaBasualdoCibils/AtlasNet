#pragma once

#include <algorithm>
namespace AtlasNet
{
template <std::size_t N> struct FixedString
{
  char value[N];

  constexpr FixedString(const char (&str)[N])
  {
    std::copy_n(str, N, value);
  }
  constexpr bool operator==(const FixedString& other) const
  {
    return std::equal(std::begin(value), std::end(value), std::begin(other.value));
  }
};
} // namespace AtlasNet