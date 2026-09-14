#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Intent/IntentRecepient.hpp"
#include <cstdint>
namespace AtlasNet::Network::Intent
{

enum class IntentAckMode : uint8_t
{
  None = 0b00,    // fire and forget
  Request = 0b01, // please acknowledge receipt
  Response = 0b11 // this packet is an acknowledgement
};

struct IntentHeader
{
  AtlasNetNodeID source;
  uint8_t ttl = 0;
  uint64_t correlationID;
  IntentMethod method;
  IntentAckMode ackMode;
  VIntent intent = Recepient::InvalidRecepient{};
  uint16_t payloadBytes = 0;

  template <typename Archive> void serialize(Archive& archive)
  {
    archive(source, ttl, method, correlationID, ackMode, intent, payloadBytes);
  }
  constexpr bool IsValid() const noexcept
  {
    if (std::holds_alternative<Recepient::InvalidRecepient>(intent)) return false;
    if (method != std::visit([](auto&& arg) { return arg.Method; }, intent)) return false;
    return true;
  }
};

} // namespace AtlasNet::Network::Intent