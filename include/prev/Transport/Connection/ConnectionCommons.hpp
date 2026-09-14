#pragma once

#include <boost/describe/enum.hpp>
#include <cstdint>
namespace AtlasNet::Network
{
enum class ConnectionState : uint8_t
{
  None = 0,
  Connecting = 1,
  Connected = 2,
  ClosedByPeer = 3,
  ProblemDetectedLocally = 4,
  INVALID = 5
};
BOOST_DESCRIBE_ENUM(ConnectionState, None, Connecting, Connected, ClosedByPeer,
                    ProblemDetectedLocally, INVALID)
} // namespace AtlasNet::Network