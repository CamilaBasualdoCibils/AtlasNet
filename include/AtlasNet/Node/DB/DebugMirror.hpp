#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Node/RPC/Database.hpp"

namespace AtlasNet::DB::DebugMirror
{
inline _Json ToJSON(const RPC::Database::RegisterNodeRequest& request)
{
  _Json capabilities = _Json::array();
  const auto addCapability =
      [&](NodeCapability capability, std::string_view name)
  {
    if (HasCapability(request.capabilities, capability))
      capabilities.push_back(name);
  };

  addCapability(NodeCapability::Shard, "Shard");
  addCapability(NodeCapability::ClientIngress, "ClientIngress");
  addCapability(NodeCapability::ControllerEligible, "ControllerEligible");
  addCapability(NodeCapability::Database, "Database");

  return {
      {"nodeID", request.nodeID.to_string()},
      {"handshakeAddress", request.handshakeAddress.to_string()},
      {"channelBusAddress", request.channelBusAddress.to_string()},
      {"macAddress", request.macAddress.ToString()},
      {"capabilityMask", static_cast<uint64_t>(request.capabilities)},
      {"capabilities", std::move(capabilities)},
  };
}
} // namespace AtlasNet::DB::DebugMirror
