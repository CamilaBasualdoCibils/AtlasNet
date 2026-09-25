#pragma once

#include "AtlasNet/Core/CmdSig/Command.hpp"

namespace AtlasNet::CmdSig
{

/** Forwards a command to the owning shard when not local (AN-A-1 relay). */
class ICommandTransport
{
public:
  virtual ~ICommandTransport() = default;
  virtual CommandAck Forward(const TransitCommandEnvelope& envelope) = 0;
};

} // namespace AtlasNet::CmdSig
