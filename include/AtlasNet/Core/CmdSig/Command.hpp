#pragma once

#include "AtlasNet/Core/CmdSig/Ordering.hpp"
#include "AtlasNet/Core/Core.hpp"
#include <boost/container/small_vector.hpp>
#include <boost/describe/enum.hpp>
#include <boost/static_string/static_string.hpp>
#include <cstdint>
#include <string>

namespace AtlasNet::CmdSig
{

inline constexpr std::size_t kCommandMaxNameLength = 64;

enum class CommandDeliveryGuarantee : uint8_t
{
  NoDelay = 0,
  Unreliable = 1,
  Reliable = 2,
  Invalid = 0xFF
};
BOOST_DESCRIBE_ENUM(CommandDeliveryGuarantee, NoDelay, Unreliable, Reliable,
                    Invalid);

enum class CommandAckStatus : uint8_t
{
  Ok = 0,
  Forwarded = 1,
  NotActor = 2,
  NotFound = 3,
  StaleSequence = 4,
  Rejected = 5,
  Invalid = 0xFF
};
BOOST_DESCRIBE_ENUM(CommandAckStatus, Ok, Forwarded, NotActor, NotFound,
                    StaleSequence, Rejected, Invalid);

struct CommandAck
{
  CommandAckStatus status = CommandAckStatus::Invalid;
  OrderingKey ordering{};

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(status, ordering);
  }
};

struct CommandPayload
{
  boost::static_string<kCommandMaxNameLength> commandName;
  boost::container::small_vector<uint8_t, 64> payload;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(commandName, payload);
  }
};

struct CommandPackage
{
  CommandPayload commandPayload;
  CommandDeliveryGuarantee deliveryMode = CommandDeliveryGuarantee::Reliable;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(commandPayload, deliveryMode);
  }
};

/** Wire envelope: actor-targeted command (AN-A-1). */
struct TransitCommandEnvelope
{
  AtlasNetEntityID targetActor{};
  OrderingKey ordering{};
  CommandPackage package{};

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(targetActor, ordering, package);
  }
};

} // namespace AtlasNet::CmdSig
