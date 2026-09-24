#include "AtlasNet/Core/Network/RPC/LocalShardRPC.hpp"
#include "AtlasNet/Node/Module/ModuleLoader.hpp"
#include <atomic>
#include <csignal>
#include <stdexcept>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>
namespace
{
std::atomic_bool stopping = false;
void Stop(int) { stopping = true; }
}
int main(int argc, char** argv)
{
  try
  {
    int ipc = -1;
    uint32_t rawShard = 0;
    std::vector<std::string> modules;
    for (int i = 1; i < argc; ++i)
    {
      const std::string_view arg(argv[i]);
      if (arg == "--ipc-fd" && ++i < argc)
        ipc = std::stoi(argv[i]);
      else if (arg == "--shard-id" && ++i < argc)
        rawShard = static_cast<uint32_t>(std::stoul(argv[i]));
      else if (arg == "--module" && ++i < argc)
        modules.emplace_back(argv[i]);
      else
        throw std::invalid_argument("Invalid shard-worker arguments");
    }
    if (ipc < 0)
      throw std::invalid_argument("Missing shard-worker IPC descriptor");
    const pid_t parent = getppid();
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() != parent) return 1;
    signal(SIGTERM, Stop);
    signal(SIGINT, Stop);
    AtlasNet::Module::ModuleLoader loader;
    AtlasNet::Module::ModuleRegistry registry;
    for (const auto& module : modules) loader.Load(module, registry);
    if (!registry.HasShardProvider())
      throw std::runtime_error("No module provides shard logic");
    auto logic = registry.GetShardProvider()->CreateShardLogic();
    if (!logic) throw std::runtime_error("Shard provider returned no shard logic");
    const AtlasNet::ShardID shard(rawShard);
    AtlasNet::Network::RPC::LocalShardRPC rpc("ShardWorkerRPC", shard, ipc);
    logic->Start(shard, rpc);
    while (!stopping.load())
    {
      rpc.Poll(AtlasNet::Network::PollType::NonBlocking);
      logic->Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    logic->Shutdown();
    return 0;
  }
  catch (...) { return 1; }
}
