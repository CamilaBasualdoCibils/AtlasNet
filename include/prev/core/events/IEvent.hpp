#pragma once

#include "atlasnet/core/utils/MacroConcepts.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include <cstdint>
#include <string_view>
// =====================================================
// Stable hash (event ID)
// =====================================================

constexpr static uint64_t ATLASNET_FNV_OFFSET = 14695981039346656037ull;
constexpr static uint64_t ATLASNET_FNV_PRIME = 1099511628211ull;

constexpr static uint64_t fnv1a64(std::string_view str)
{
  uint64_t hash = ATLASNET_FNV_OFFSET;
  for (char c : str)
  {
    hash ^= static_cast<uint64_t>(c);
    hash *= ATLASNET_FNV_PRIME;
  }
  return hash;
}

namespace AtlasNet
{
using EventID = uint64_t;

struct IEvent
{
  virtual ~IEvent() = default;
};
} // namespace AtlasNet

// =====================================================
// Event field encoding
// =====================================================

#define ATLASNET_EVENT_FIELD(type, name) (type, name)

// Expand a single (type, name)
#define ATLASNET_EVENT_FIELD_DECL_I(type, name) type name;
#define ATLASNET_EVENT_ARG_I(type, name) type name
#define ATLASNET_EVENT_INIT_I(type, name) name(name)

// wrappers for FOR_EACH
#define ATLASNET_EVENT_FIELD_DECL(tuple) ATLASNET_EVENT_FIELD_DECL_I tuple
#define ATLASNET_EVENT_ARG(tuple) ATLASNET_EVENT_ARG_I tuple
#define ATLASNET_EVENT_INIT(tuple) ATLASNET_EVENT_INIT_I tuple

#define ATLASNET_EVENT_FIELD_SERIALIZE_I(type, name) writer(name);
#define ATLASNET_EVENT_FIELD_SERIALIZE(tuple)                                  \
  ATLASNET_EVENT_FIELD_SERIALIZE_I tuple

#define ATLASNET_EVENT_FIELD_DESERIALIZE_I(type, name) reader(name);
#define ATLASNET_EVENT_FIELD_DESERIALIZE(tuple)                                \
  ATLASNET_EVENT_FIELD_DESERIALIZE_I tuple

// =====================================================
// Event macro
// =====================================================

#define ATLASNET_EVENT(Name, ...)                                              \
  struct Name : public ::AtlasNet::IEvent                                      \
  {                                                                            \
    ATLASNET_FOR_EACH(ATLASNET_EVENT_FIELD_DECL, ATLASNET_SEP_NONE,            \
                      __VA_ARGS__)                                             \
                                                                               \
    Name(ATLASNET_FOR_EACH(ATLASNET_EVENT_ARG, ATLASNET_SEP_COMMA,             \
                           __VA_ARGS__))                                       \
        : ATLASNET_FOR_EACH(ATLASNET_EVENT_INIT, ATLASNET_SEP_COMMA,           \
                            __VA_ARGS__)                                       \
    {                                                                          \
    }                                                                          \
    Name() {}                                                                  \
    void Serialize(AtlasNet::ByteWriter& writer) const                         \
    {                                                                          \
      ATLASNET_FOR_EACH(ATLASNET_EVENT_FIELD_SERIALIZE, ATLASNET_SEP_NONE,     \
                        __VA_ARGS__)                                           \
    }                                                                          \
    void Deserialize(AtlasNet::ByteReader& reader)                             \
    {                                                                          \
      ATLASNET_FOR_EACH(ATLASNET_EVENT_FIELD_DESERIALIZE, ATLASNET_SEP_NONE,   \
                        __VA_ARGS__)                                           \
    }                                                                          \
                                                                               \
    static constexpr ::AtlasNet::EventID ID = fnv1a64(#Name);                  \
  }
;