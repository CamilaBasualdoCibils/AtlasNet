#pragma once
#include "AtlasNet/Core/Types/FixedString.hpp"
#include <boost/pfr.hpp>
namespace AtlasNet::Network::Messages
{
namespace Detail
{
inline consteval uint64_t HashMessageName(std::string_view str)
{
  uint64_t hash = 14695981039346656037ull; // FNV offset basis

  for (char c : str)
  {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ull;
  }

  return hash;
}
template <typename Archive, typename T>
concept HasMemberSerialize =
    requires(Archive& ar, T& value) { value.serialize(ar); };

template <typename Archive, typename T>
concept HasFreeSerialize = requires(Archive& ar, T& value) { ar(value); };
} // namespace Detail
using MessageID = uint64_t;
template <FixedString Name, typename Payload> struct Message
{
  using PayloadType = Payload;
  static constexpr auto NameValue = Name;
  static constexpr uint64_t NameHash =
      Detail::HashMessageName(NameValue.c_str());
  PayloadType payload;
  template <typename archive> void serialize(archive& ar)
  {
    static_assert(Detail::HasMemberSerialize<archive, PayloadType>,
                  "Payload type must have a serialize method or a free "
                  "serialize function");
    payload.serialize(ar);
  }
};
}; // namespace AtlasNet::Network::Messages
