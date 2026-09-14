#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/RPC/NetworkTransportRPC.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/Core/Service/AtlasNetService.hpp"
#include "AtlasNet/DB/Handshake.hpp"
#include "valkeymodule.h"
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace AtlasNet
{
class AtlasNetDB : public AtlasNetService
{
  ValkeyModuleCtx* g_ctx = nullptr;

public:
  AtlasNetDB(ValkeyModuleCtx* ctx, int argc, char** argv)
      : AtlasNetService(AtlasNetServiceType::DB, argc, argv)
  {
    g_ctx = ctx;
  }

private:
  void Initialize() override;
  void Tick() override;

  ValkeyModuleCtx* GetValkeyCtx()
  {
    return g_ctx;
  }
  void RegisterHandshakeBinds()
  {

    GetHandshakeRPC().Bind<DB::RPC_DB_Ping>(
        [this](const Network::RPC::NetworkTransportRPC::CallContext&,
               int) -> int
        {
          GetLogger()->info("Received ping request");
          return 0;
        });
    GetHandshakeRPC().Bind<DB::RPC_DB_RegisterNode>(
        [this](
            const Network::RPC::NetworkTransportRPC::CallContext&,
            const DB::RegisterNodeRequest& request) -> DB::RegisterNodeResponse
        { return RegisterNode(request); });
  }
  struct RegisteredNode
  {
    AtlasNetNodeID nodeID;
    Network::SocketAddress handshakeAddress, channelBusAddress;
    Network::MACAddress macAddress;
    std::chrono::steady_clock::time_point timestamp;
    template <typename Archive> void serialize(Archive& ar)
    {
      ar(nodeID, handshakeAddress, channelBusAddress, macAddress, timestamp);
    }
  };
  DB::RegisterNodeResponse RegisterNode(const DB::RegisterNodeRequest& request)
  {
    ValkeyModule_ThreadSafeContextLock(GetValkeyCtx());

    RegisteredNode node{.nodeID = request.nodeID,
                        .handshakeAddress = request.handshakeAddress,
                        .channelBusAddress = request.channelBusAddress,
                        .macAddress = request.macAddress,
                        .timestamp = std::chrono::steady_clock::now()};
    NetBinaryWriter writer;
    writer(node);
    std::vector<std::byte> data = writer.Release();

    ValkeyModuleString* hashKey =
        ValkeyModule_CreateString(GetValkeyCtx(), "AtlasNet:RegisteredNodes",
                                  sizeof("AtlasNet:RegisteredNodes") - 1);

    const std::string nodeID = request.nodeID.to_string();

    ValkeyModuleString* field =
        ValkeyModule_CreateString(GetValkeyCtx(), nodeID.data(), nodeID.size());

    // Value = binary serialized RegisteredNode
    ValkeyModuleString* value = ValkeyModule_CreateString(
        GetValkeyCtx(), reinterpret_cast<const char*>(data.data()),
        data.size());

    ValkeyModuleKey* key =
        ValkeyModule_OpenKey(GetValkeyCtx(), hashKey, VALKEYMODULE_WRITE);

    DB::RegisterNodeResponse response{};

    if (ValkeyModule_HashSet(key, VALKEYMODULE_HASH_NONE, field, value,
                             nullptr) == VALKEYMODULE_ERR)
    {
      GetLogger()->error("Failed to register node {}",
                         request.nodeID.to_string());

      // Set response error however your RPC response represents it
    }
    else
    {
      GetLogger()->info("Registered node with ID: {}",
                        request.nodeID.to_string());
    }

    ValkeyModule_CloseKey(key);

    ValkeyModule_FreeString(GetValkeyCtx(), value);
    ValkeyModule_FreeString(GetValkeyCtx(), field);
    ValkeyModule_FreeString(GetValkeyCtx(), hashKey);

    ValkeyModule_ThreadSafeContextUnlock(GetValkeyCtx());

    return response;
  }
};
}; // namespace AtlasNet