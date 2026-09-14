#pragma once

#include <charconv>
#include <stdexcept>
#include <string>
#include <functional>

namespace AtlasNet
{

template <typename underlying_type, typename tag> struct StrongTypedef
{
  using underlying_type_t = underlying_type;
  explicit StrongTypedef(underlying_type value) : value(value) {}
  StrongTypedef(const StrongTypedef&) = default;
  StrongTypedef& operator=(const StrongTypedef&) = default;
  StrongTypedef() : value() {}
  underlying_type value;

  operator underlying_type() const
  {
    return value;
  }

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(value);
  }
  std::string to_string() const
  {
    return std::to_string(value);
  }
  static StrongTypedef from_string(const std::string_view& str)
  {
    StrongTypedef obj(underlying_type{});
    auto [ptr, ec] =
        std::from_chars(str.data(), str.data() + str.size(), obj.value);
    if (ec != std::errc{})
    {
      throw std::runtime_error("Failed to convert string to StrongTypedef");
    }
    return obj;
  }

  std::ofstream& operator<<(std::ofstream& os) const
  {
    os << value;
    return os;
  }
};

} // namespace AtlasNet

namespace std
{
template <typename underlying_type, typename tag>
struct hash<AtlasNet::StrongTypedef<underlying_type, tag>>
{
  std::size_t operator()(const AtlasNet::StrongTypedef<underlying_type, tag>& obj) const
  {
    return std::hash<underlying_type>()(obj.value);
  }
};
} // namespace std