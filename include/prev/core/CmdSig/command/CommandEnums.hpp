#pragma once

#include "atlasnet/core/messages/MessageSystem.hpp"
#include "boost/describe/enum.hpp"
#include "steam/steamtypes.h"
#include <cstdint>
namespace AtlasNet
{

/* enum class CommandSendMode : uint8
{
  eReliable, // Reliable command, guaranteed to be delivered, sent as soon as
             // possible

  eReliableBatched, // Reliable command, batched together with other reliable
                    // commands

  eUnreliable, // Unreliable command, may be lost, sent as soon as possible

  eUnreliableBatched, // Unreliable command, batched together with other
                      // unreliable commands

  eNoDelay, // Command sent immediately without delay or dropped

  eInvalid
}; */

static constexpr uint8_t CommandDeliveryBatchedBit = 1 << 4;
enum class CommandDeliveryGuarantee : uint8_t
{
  NoDelay = 0b000, // Send immediately or drop.
                   // No retries or acknowledgements.

  Unreliable = 1 << 0, // Retry enough to get onto the wire.
                       // Arrival is not guaranteed.
  UnreliableBatched =
      Unreliable + CommandDeliveryBatchedBit, // Like Unreliable, but may be
                                              // batched with other commands,
                                              // introducing additional delay.

  GatewayConfirmed = 1 << 1, // Guaranteed received by the AtlasNet gateway.
  GatewayConfirmedBatched = GatewayConfirmed + CommandDeliveryBatchedBit,

  ServerConfirmed = 1 << 2, // Guaranteed received by the owning shard/server
                            // through the AtlasNet network.
  ServerConfirmedBatched = ServerConfirmed + CommandDeliveryBatchedBit,

  Invalid = 0xFF
};
BOOST_DESCRIBE_ENUM(CommandDeliveryGuarantee, NoDelay, Unreliable,
                    UnreliableBatched, GatewayConfirmed,
                    GatewayConfirmedBatched, ServerConfirmed,
                    ServerConfirmedBatched, Invalid);
inline MessageSendMode CommandDeliveryToMessageSendMode(CommandDeliveryGuarantee deliveryMode)
{
   MessageSendMode sendMode = MessageSendMode::eINVALID;

    switch (deliveryMode)
    {

    case CommandDeliveryGuarantee::NoDelay:
      sendMode = MessageSendMode::eNoDelay;
      break;
    case CommandDeliveryGuarantee::Unreliable:
      sendMode = MessageSendMode::eUnreliable;
      break;
    case CommandDeliveryGuarantee::UnreliableBatched:
      sendMode = MessageSendMode::eUnreliableBatched;
      break;
    case CommandDeliveryGuarantee::GatewayConfirmed:
    case CommandDeliveryGuarantee::ServerConfirmed:
      sendMode = MessageSendMode::eReliable;
      break;
    case CommandDeliveryGuarantee::GatewayConfirmedBatched:
    case CommandDeliveryGuarantee::ServerConfirmedBatched:
      sendMode = MessageSendMode::eReliableBatched;
      break;
    case CommandDeliveryGuarantee::Invalid:
      sendMode = MessageSendMode::eINVALID;
      break;
    }
    return sendMode;
}
enum class CommandAckStatus : uint8_t
{
  Dropped, // Command was dropped and will not be delivered.
  Sent,    // Command was sent but not yet acknowledged.
  GatewayAck,
  ServerAck,
  TimedOut, // Command was not acknowledged within the expected time frame.
  InvalidCommand,
  Disconnected,
  UnknownError,
  Invalid,
};
BOOST_DESCRIBE_ENUM(CommandAckStatus, Dropped, Sent, GatewayAck, ServerAck,
                    TimedOut, InvalidCommand, Disconnected, UnknownError, Invalid);

} // namespace AtlasNet