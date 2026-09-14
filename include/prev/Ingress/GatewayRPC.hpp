#pragma once

#include "atlasnet/core/CmdSig/command/Command.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"

namespace AtlasNet
{
    using GatewayRPC_IngressCommand = RPC<"Gateway_IngressCommand", CommandAck, IngressCommandEnvelope>;

}