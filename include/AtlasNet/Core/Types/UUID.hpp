#pragma once

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <compare>
#include <functional>
#include <string>
#include <string_view>

namespace AtlasNet
{

class UUID
{
  boost::uuids::uuid id;

public:
  UUID() : id() {}

  explicit UUID(const boost::uuids::uuid& uuid) : id(uuid) {}
  explicit UUID(const std::string_view uuid_str)
      : id(boost::uuids::string_generator()(uuid_str.data()))
  {
  }

  bool operator==(const UUID& other) const
  {
    return id == other.id;
  }

  std::strong_ordering operator<=>(const UUID& other) const
  {
    return std::lexicographical_compare_three_way(
        id.begin(), id.end(), other.id.begin(), other.id.end());
  }

  std::string to_string() const
  {
    return boost::uuids::to_string(id);
  }
  std::string to_short_string() const
  {
    std::string str = to_string();
    return str.substr(0, 8);
  }
  static UUID from_string(const std::string_view uuid_str)
  {
    return UUID(boost::uuids::string_generator()(uuid_str.data()));
  }

  static UUID Generate()
  {
    static boost::uuids::random_generator generator;
    return UUID(generator());
  }

  uint8_t operator[](size_t index) const
  {
    return id.data[index];
  }

  void* data()
  {
    return id.data;
  }

  const void* data() const
  {
    return id.data;
  }

  size_t size() const
  {
    return sizeof(id.data);
  }
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(id.data[0], id.data[1], id.data[2], id.data[3], id.data[4], id.data[5],
       id.data[6], id.data[7], id.data[8], id.data[9], id.data[10], id.data[11],
       id.data[12], id.data[13], id.data[14], id.data[15]);
  }
};

template <typename Tag> struct StrongUUID : public UUID
{
  StrongUUID() = default;
  explicit StrongUUID(UUID v) : UUID(std::move(v)) {}

  friend bool operator==(const StrongUUID&, const StrongUUID&) = default;
};

} // namespace AtlasNet

namespace std
{
template <> struct hash<AtlasNet::UUID>
{
  size_t operator()(const AtlasNet::UUID& uuid) const noexcept
  {
    const auto* bytes = static_cast<const uint8_t*>(uuid.data());
    size_t h = 0;

    for (size_t i = 0; i < uuid.size(); ++i)
    {
      h ^= static_cast<size_t>(bytes[i]) + 0x9e3779b9u + (h << 6) + (h >> 2);
    }

    return h;
  }
};

template <typename Tag> struct hash<AtlasNet::StrongUUID<Tag>>
{
  size_t operator()(const AtlasNet::StrongUUID<Tag>& uuid) const noexcept
  {
    return hash<AtlasNet::UUID>{}(static_cast<const AtlasNet::UUID&>(uuid));
  }
};
} // namespace std