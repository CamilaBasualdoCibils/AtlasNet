#pragma once
#include "CommandEnums.hpp"
#include "atlasnet/core/CmdSig/command/CommandEnums.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/utils/FixedString.hpp"
#include "boost-src/libs/container/include/boost/container/small_vector.hpp"
#include "boost-src/libs/static_string/include/boost/static_string/static_string.hpp"
#include <atlasnet/core/entity/Entity.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/static_string.hpp>
#include <cstdint>
#include <string>
namespace AtlasNet
{
const static size_t COMMAND_MAX_NAME_LENGTH = 64;
struct CommandAck
{
  CommandAckStatus status;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(status);
  }
};
struct CommandPayload
{
  boost::static_string<COMMAND_MAX_NAME_LENGTH> commandName;
  boost::container::small_vector<uint8_t, 64> payload;

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(commandName, payload);
  }
};
struct CommandPackage
{
  CommandPayload commandPayload;
  CommandDeliveryGuarantee deliveryMode;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(commandPayload);
    ar(deliveryMode);
  }
};
struct IngressCommandEnvelope
{
  CommandPackage package;

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(package);
  }
};
/* ATLASNET_MESSAGE(IngreeCommandMessage,
                 ATLASNET_MESSAGE_DATA(CommandPackage, commandPackage)); */
/* struct IngressCommandEnvelope
{
  CommandPackage commandPackage;

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(commandPackage);
  }
}; */
struct TransitCommandEnvelope
{
  AtlasNetEntityID targetEntity;
  CommandPackage commandPackage;

  template <typename Archive> void serialize(Archive& ar)
  {
    ar(targetEntity, commandPackage);
  }
};

struct ICommand
{
  virtual void Serialize(ByteWriter& serializer) const = 0;
  virtual void Deserialize(ByteReader& deserializer) = 0;
};
struct ISignal
{
  virtual void Serialize(ByteWriter& serializer) const = 0;
  virtual void Deserialize(ByteReader& deserializer) = 0;
};
#define ATLASNET_COMMAND_DATA(Type, Name) (Type, Name)
#define ATLASNET_COMMAND(Namespace, Name, ...)                                 \
  struct Name : AtlasNet::ICommand                                             \
  {                                                                            \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, ATLASNET_SEP_NONE, __VA_ARGS__)  \
                                                                               \
    static inline std::string GetName()                                        \
    {                                                                          \
      return std::string(#Namespace) + '.' + #Name;                            \
    }                                                                          \
    void Serialize(AtlasNet::ByteWriter& archive) const override               \
    {                                                                          \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
                                                                               \
    void Deserialize(AtlasNet::ByteReader& archive) override                   \
    {                                                                          \
                                                                               \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
  };
#define ATLASNET_SIGNAL_DATA(Type, Name) (Type, Name)
#define ATLASNET_SIGNAL(Namespace, Name, ...)                                  \
  struct Name : AtlasNet::ISignal                                              \
  {                                                                            \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, ATLASNET_SEP_NONE, __VA_ARGS__)  \
                                                                               \
    static inline std::string GetName()                                        \
    {                                                                          \
      return std::string(#Namespace) + '.' + #Name;                            \
    }                                                                          \
    void Serialize(AtlasNet::ByteWriter& archive) const override               \
    {                                                                          \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
                                                                               \
    void Deserialize(AtlasNet::ByteReader& archive) override                   \
    {                                                                          \
                                                                               \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE,           \
                        __VA_ARGS__)                                           \
    }                                                                          \
  };

// Example Command
ATLASNET_COMMAND(ExampleNamespace, ExampleCommand,
                 ATLASNET_COMMAND_DATA(int, exampleInt),
                 ATLASNET_COMMAND_DATA(std::string, exampleString));
// Example Signal
ATLASNET_SIGNAL(ExampleNamespace, ExampleSignal,
                ATLASNET_SIGNAL_DATA(int, exampleInt),
                ATLASNET_SIGNAL_DATA(std::string, exampleString));

/*
struct ExternalCommandEnvelope
{
  const static size_t MaxCommandNameLength = 64;
  boost::static_string<MaxCommandNameLength> commandName;
  boost::container::small_vector<uint8_t, 64> payload;

  void Serialize(ByteWriter& serializer) const
  {
    serializer.blob(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(commandName.data()),
        commandName.size()));
    serializer.blob(std::span<const uint8_t>(payload.data(), payload.size()));
  }
  void Deserialize(ByteReader& deserializer)
  {
    std::span<const uint8_t> commandNameSpan;
    deserializer.blob(commandNameSpan);
    commandName.assign(reinterpret_cast<const char*>(commandNameSpan.data()),
                       commandNameSpan.size());
    std::span<const uint8_t> payloadSpan;
    deserializer.blob(payloadSpan);
    payload.assign(payloadSpan.begin(), payloadSpan.end());
  }
};

struct InternalCommandEnvelope
{
  const static size_t MaxCommandNameLength = 64;
  boost::static_string<MaxCommandNameLength> commandName;

  AtlasNetEntityID targetEntity;
  //uint64_t logical_entity_sequence;

  enum SenderType : uint8_t
  {
    AtlasNetNode = 0,
    Client = 1
  } senderType;
  std::variant<AtlasNetNodeID, AtlasNetClientID> sender;
  boost::container::small_vector<uint8_t, 64> payload;
  void Serialize(ByteWriter& serializer) const
  {
    serializer.blob(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(commandName.data()),
        commandName.size()));
    targetEntity.Serialize(serializer);
    //serializer.u64(logical_entity_sequence);
    serializer.u8(static_cast<uint8_t>(senderType));
    if (senderType == SenderType::AtlasNetNode)
    {

      serializer(std::get<AtlasNetNodeID>(sender));
    }
    else if (senderType == SenderType::Client)
    {
      serializer(std::get<AtlasNetClientID>(sender));
    }
    else
    {
      throw std::runtime_error(
          "Invalid sender type in InternalCommandEnvelope");
    }
    serializer.blob(std::span<const uint8_t>(payload.data(), payload.size()));
  }

  void Deserialize(ByteReader& deserializer)
  {
    std::span<const uint8_t> commandNameSpan;
    deserializer.blob(commandNameSpan);
    commandName.assign(reinterpret_cast<const char*>(commandNameSpan.data()),
                       commandNameSpan.size());
    targetEntity.Deserialize(deserializer);
    //deserializer.u64(logical_entity_sequence);
    deserializer.u8(reinterpret_cast<uint8_t&>(senderType));
    if (senderType == SenderType::AtlasNetNode)
    {
      AtlasNetNodeID nodeID;
      deserializer(nodeID);
      sender = nodeID;
    }
    else if (senderType == SenderType::Client)
    {
      AtlasNetClientID clientID;
      deserializer(clientID);
      sender = clientID;
    }
    else
    {
      throw std::runtime_error(
          "Invalid sender type in InternalCommandEnvelope");
    }
    std::span<const uint8_t> payloadSpan;
    deserializer.blob(payloadSpan);
    payload.assign(payloadSpan.begin(), payloadSpan.end());
  }
};
ATLASNET_MESSAGE(InternalCommandMessage,
                 ATLASNET_MESSAGE_DATA(InternalCommandEnvelope, envelope));
ATLASNET_MESSAGE(ExternalCommandMessage,
                 ATLASNET_MESSAGE_DATA(ExternalCommandEnvelope, envelope));

struct ICommandSerializable
{
  virtual void Serialize(ByteWriter& serializer) const = 0;
  virtual void Deserialize(ByteReader& deserializer) = 0;
};
struct ISignalSerializable
{
  virtual void Serialize(ByteWriter& serializer) const = 0;
  virtual void Deserialize(ByteReader& deserializer) = 0;
};
#define ATLASNET_COMMAND_DATA(Type, Name) (Type, Name)
#define ATLASNET_COMMAND(Namespace, Name, ...) \
  struct Name : AtlasNet::ICommandSerializable \
  { \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, ATLASNET_SEP_NONE, __VA_ARGS__)
\
                                                                               \
    static inline std::string GetName() \
    { \
      return std::string(#Namespace) + '.' + #Name; \
    } \
    void Serialize(AtlasNet::ByteWriter& archive) const override \
    { \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE, \
                        __VA_ARGS__) \
    } \
                                                                               \
    void Deserialize(AtlasNet::ByteReader& archive) override \
    { \
                                                                               \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE, \
                        __VA_ARGS__) \
    } \
  };
#define ATLASNET_SIGNAL_DATA(Type, Name) (Type, Name)
#define ATLASNET_SIGNAL(Namespace, Name, ...) \
  struct Name : AtlasNet::ISignalSerializable \
  { \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, ATLASNET_SEP_NONE, __VA_ARGS__)
\
                                                                               \
    static inline std::string GetName() \
    { \
      return std::string(#Namespace) + '.' + #Name; \
    } \
    void Serialize(AtlasNet::ByteWriter& archive) const override \
    { \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE, \
                        __VA_ARGS__) \
    } \
                                                                               \
    void Deserialize(AtlasNet::ByteReader& archive) override \
    { \
                                                                               \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, ATLASNET_SEP_NONE, \
                        __VA_ARGS__) \
    } \
  };

// Example Command
ATLASNET_COMMAND(ExampleNamespace, ExampleCommand,
                 ATLASNET_COMMAND_DATA(int, exampleInt),
                 ATLASNET_COMMAND_DATA(std::string, exampleString));
// Example Signal
ATLASNET_SIGNAL(ExampleNamespace, ExampleSignal,
                ATLASNET_SIGNAL_DATA(int, exampleInt),
                ATLASNET_SIGNAL_DATA(std::string, exampleString)); */

} // namespace AtlasNet
