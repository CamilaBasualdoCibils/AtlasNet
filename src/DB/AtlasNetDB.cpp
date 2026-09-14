#include "AtlasNetDB.hpp"

#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "valkeymodule.h"

#include <spdlog/spdlog.h>
#include <string>
#include <thread>
static std::jthread dbThread;
int AtlasPingCommand(ValkeyModuleCtx* ctx, ValkeyModuleString** argv, int argc)
{
  ValkeyModule_ReplyWithSimpleString(ctx, "PONG");
  return VALKEYMODULE_OK;
}

extern "C"
{

  int ValkeyModule_OnLoad(ValkeyModuleCtx* ctx, ValkeyModuleString** argv,
                          int argc)
  {

    if (ValkeyModule_Init(ctx, "atlasdb", 1, VALKEYMODULE_APIVER_1) ==
        VALKEYMODULE_ERR)
    {
      return VALKEYMODULE_ERR;
    }

    ValkeyModule_Log(ctx, "notice", "AtlasNet Valkey Module loading...");
    auto g_ctx = ValkeyModule_GetDetachedThreadSafeContext(ctx);

    dbThread = std::jthread(
        [g_ctx]()
        {
          try
          {
            std::string arg0 = "invalid/path";
            std::string args1 = "--handshake-port";
            std::string args2 =
                std::to_string(ATLASNET_DB_DEBUG_HANDSHAKE_PORT);
            const char* args[] = {arg0.c_str(), args1.c_str(), args2.c_str()};
            AtlasNet::AtlasNetDB db =
                AtlasNet::AtlasNetDB(g_ctx, 3, (char**)args);
            db.Run();
          }
          catch (const std::exception& e)
          {
            ValkeyModule_Log(g_ctx, "error", "Exception in AtlasNetDB: %s",
                             e.what());
          }
        });

    return VALKEYMODULE_OK;
  }
  int ValkeyModule_OnUnload(ValkeyModuleCtx* ctx)
  {
    ValkeyModule_Log(ctx, "notice", "AtlasNet Valkey Module unloading...");
    dbThread.request_stop();
    if (dbThread.joinable())
    {
      dbThread.join();
    }
    return VALKEYMODULE_OK;
  }
}
void AtlasNet::AtlasNetDB::Initialize()
{
  RegisterHandshakeBinds();
}
void AtlasNet::AtlasNetDB::Tick()
{
  GetHandshakeRPC().Poll(Network::PollType::NonBlocking);
}
