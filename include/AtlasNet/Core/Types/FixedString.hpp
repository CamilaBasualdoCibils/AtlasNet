#pragma once

#include <algorithm>
#include <string>
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
  operator std::string() const
  {
    return std::string(value, N-1);
  }
  operator std::string_view() const
  {
    return std::string_view(value, N-1);
  }
};
} // namespace AtlasNet