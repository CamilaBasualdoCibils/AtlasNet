#include "AtlasNetDB.hpp"
#include "ValkeyModuleBackend.hpp"
#include <charconv>

namespace
{
std::unique_ptr<AtlasNet::AtlasNetNode> node;
int pollDescriptor = -1;
ValkeyModuleCtx* context = nullptr;

void Tick(int, void*, int)
{
  try
  {
    node->Poll();
  }
  catch (const std::exception& e)
  {
    ValkeyModule_Log(context, "warning", "AtlasNet poll: %s", e.what());
  }
}
} // namespace

extern "C" int ValkeyModule_OnLoad(ValkeyModuleCtx* ctx,
                                   ValkeyModuleString** argv, int argc)
{
  if (ValkeyModule_Init(ctx, "atlasdb", 1, VALKEYMODULE_APIVER_1) ==
      VALKEYMODULE_ERR)
    return VALKEYMODULE_ERR;
  try
  {
    AtlasNet::NodeConfig config;
    config.capabilities = AtlasNet::NodeCapability::Database;
    config.transport.handshakeListenPort = ATLASNET_DB_DEBUG_HANDSHAKE_PORT;
    // Optional module argument: handshake port. Never read process environment.
    if (argc > 1)
      throw std::invalid_argument(
          "Expected at most one handshake port argument");
    if (argc == 1)
    {
      size_t length;
      const char* value = ValkeyModule_StringPtrLen(argv[0], &length);
      unsigned port = 0;
      auto [end, error] = std::from_chars(value, value + length, port);
      if (error != std::errc{} || end != value + length || port == 0 ||
          port > 65535)
        throw std::invalid_argument("Invalid handshake port");
      config.transport.handshakeListenPort = static_cast<uint16_t>(port);
    }
    context = ValkeyModule_GetDetachedThreadSafeContext(ctx);
    node = std::make_unique<AtlasNet::AtlasNetNode>(
        config, std::make_unique<AtlasNet::DB::ValkeyModuleBackend>(context));
    // Startup and polling execute on the Valkey thread: no locks, TCP loopback,
    // process signal handlers, or worker joins during module unload.
    node->Start();
    const int descriptor = node->GetHandshakePollDescriptor();
    if (descriptor < 0 ||
        ValkeyModule_EventLoopAdd(descriptor, VALKEYMODULE_EVENTLOOP_READABLE,
                                  Tick, nullptr) != VALKEYMODULE_OK)
      throw std::runtime_error(
          "Failed to register AtlasNet socket with Valkey event loop");
    pollDescriptor = descriptor;
    return VALKEYMODULE_OK;
  }
  catch (const std::exception& e)
  {
    ValkeyModule_Log(ctx, "warning", "AtlasNet initialization failed: %s",
                     e.what());
    pollDescriptor = -1;
    node.reset();
    if (context)
      ValkeyModule_FreeThreadSafeContext(context);
    context = nullptr;
    return VALKEYMODULE_ERR;
  }
}

extern "C" int ValkeyModule_OnUnload(ValkeyModuleCtx* ctx)
{
  if (pollDescriptor >= 0)
    ValkeyModule_EventLoopDel(pollDescriptor, VALKEYMODULE_EVENTLOOP_READABLE);
  pollDescriptor = -1;
  node.reset();
  if (context)
    ValkeyModule_FreeThreadSafeContext(context);
  context = nullptr;
  return VALKEYMODULE_OK;
}
