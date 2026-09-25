#pragma once

#include "AtlasNet/Core/CmdSig/Signal.hpp"

namespace AtlasNet::CmdSig
{

/**
 * Client-bound signal egress seam.
 * Full signal listener is deferred; V1 uses capture/mocks in tests.
 */
class ISignalEgress
{
public:
  virtual ~ISignalEgress() = default;
  virtual void Emit(const TransitSignalEnvelope& signal) = 0;
};

} // namespace AtlasNet::CmdSig
