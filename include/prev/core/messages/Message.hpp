#pragma once

#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/utils/FixedString.hpp"
#include "atlasnet/core/utils/MacroConcepts.hpp"
#include <functional>

namespace AtlasNet
{
using MessageID = std::size_t;

class IMessage
{
public:
  static MessageID DeserializeTypeIdHash(ByteReader& archive)
  {
    MessageID typeIdHash;
    archive(typeIdHash);
    return typeIdHash;
  }
};
template <FixedString _FieldName, typename T> struct MessageField
{
  static constexpr auto Name = _FieldName;
  static constexpr auto FieldName = _FieldName;

  using Type = T;
};

template <FixedString Name, typename... Fields> struct Message : IMessage
{
  using FieldsTuple = std::tuple<typename Fields::Type...>;
  FieldsTuple fields;

  static constexpr auto NameString = Name;

  static constexpr MessageID Id =
      std::hash<std::string_view>{}(NameString.value);


  template <size_t Index>
    auto& Get()
  {
    return std::get<Index>(fields);
  }

  template <size_t Index>
    const auto& Get() const
  {
    return std::get<Index>(fields);
  }

  void Serialize(ByteWriter& archive) const
  {
    archive(Id);
    // archive(fields);
  }
  void Deserialize(ByteReader& archive)
  {
    MessageID typeIdHash = DeserializeTypeIdHash(archive);
    if (typeIdHash != Id)
    {
      throw std::runtime_error(
          "Message type ID mismatch during deserialization");
    }
    std::apply([&archive](auto&... args) { archive(args...); }, fields);
  }
  template <typename Archive>
    void serialize(Archive& ar)
    {
        std::apply(
            [&ar](auto&... args)
            {
                ar(args...);
            },
            fields);
    }
};
using TesTMessage = Message<"TestMessage", MessageField<"field1", int>,
                            MessageField<"field2", std::string>>;


/* template <FixedString Name, typename... Args>
struct Message : AtlasNet::IMessage
{
  using ArgsTuple = std::tuple<Args...>;
  static constexpr auto NameString = Name;
  static constexpr MessageID Id =
      std::hash<std::string_view>{}(NameString.value);
}; */

} // namespace AtlasNet

// =====================================================
// user-facing field syntax
// =====================================================
#define ATLASNET_MESSAGE_DATA(Type, Name) (Type, Name)

// =====================================================
// unwrap (Type, Name)
// =====================================================

#define ATLASNET_DECLARE_FIELD(Field) ATLASNET_DECLARE_FIELD_I Field
#define ATLASNET_DECLARE_FIELD_I(Type, Name) Type Name;

#define ATLASNET_SERIALIZE_FIELD(Field) ATLASNET_SERIALIZE_FIELD_I Field
#define ATLASNET_SERIALIZE_FIELD_I(Type, Name) archive(Name);

#define ATLASNET_MESSAGE_SERIALIZE_FIELD(Field)                                \
  ATLASNET_MESSAGE_SERIALIZE_FIELD_I Field
#define ATLASNET_MESSAGE_SERIALIZE_FIELD_I(Type, Name) Name

// =====================================================
// hash name (unchanged logic, but FIXED consistency issue)
// =====================================================

#define ATLASNET_MESSAGE_HASH_NAME(Name)                                       \
  const static inline std::string GetName()                                    \
  {                                                                            \
    return #Name;                                                              \
  }                                                                            \
  const static inline AtlasNet::MessageID TypeIdHash =                         \
      std::hash<std::string_view>{}(GetName());

// =====================================================
// final message macro (UPDATED to new macro system)
// =====================================================

#define ATLASNET_MESSAGE(Name, ...)                                            \
  struct Name : AtlasNet::IMessage                                             \
  {                                                                            \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, ATLASNET_SEP_NONE, __VA_ARGS__)  \
                                                                               \
    ATLASNET_MESSAGE_HASH_NAME(Name);                                          \
                                                                               \
    void Serialize(AtlasNet::ByteWriter& archive) const                        \
    {                                                                          \
      archive(TypeIdHash);                                                     \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
                                                                               \
    void Deserialize(AtlasNet::ByteReader& archive)                            \
    {                                                                          \
      AtlasNet::MessageID typeIdHash =                                         \
          AtlasNet::IMessage::DeserializeTypeIdHash(archive);                  \
      (void)typeIdHash;                                                        \
                                                                               \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
    template <class Archive> void serialize(Archive& archive)                  \
    {                                                                          \
      archive(TypeIdHash, ATLASNET_FOR_EACH(ATLASNET_MESSAGE_SERIALIZE_FIELD,  \
                                            ATLASNET_SEP_COMMA, __VA_ARGS__)); \
    }                                                                          \
  };