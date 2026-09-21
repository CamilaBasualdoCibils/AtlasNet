#pragma once

#include "AtlasNet/Core/CmdSig/Ordering.hpp"
#include "AtlasNet/Core/Core.hpp"
#include <boost/container/small_vector.hpp>
#include <boost/static_string/static_string.hpp>
#include <cstdint>

namespace AtlasNet::CmdSig
{

inline constexpr std::size_t kSignalMaxNameLength = 64;

/** Client-bound signal (AtlasNet → client). Listener plugs in later. */
struct SignalPayload
{
  boost::static_string<kSignalMaxNameLength> signalName;
  boost::container::small_vector<uint8_t, 64> payload;

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(signalName, payload);
  }
};

struct TransitSignalEnvelope
{
  AtlasNetClientID targetClient{};
  AtlasNetEntityID sourceActor{};
  OrderingKey ordering{};
  SignalPayload payload{};

  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(targetClient, sourceActor, ordering, payload);
  }
};

} // namespace AtlasNet::CmdSig
