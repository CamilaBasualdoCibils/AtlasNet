#include "AtlasNet/Node/Shard/ShardManager.hpp"
#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace
{
std::atomic_uint64_t nextManagerName{0};
}
AtlasNet::ShardManager::ShardManager(Config value)
    : config(std::move(value)), rpc("NodeShardRPC:" + std::to_string(nextManagerName++))
{
  if (config.workerExecutable.empty())
    throw std::invalid_argument("Shard worker executable is required");
}
AtlasNet::ShardManager::~ShardManager() { Shutdown(); }
AtlasNet::ShardID AtlasNet::ShardManager::CreateShard()
{
  const ShardID id(nextShardID++);
  int pair[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair) != 0)
    throw std::runtime_error("Failed to create shard socketpair");
  const pid_t process = fork();
  if (process < 0)
  {
    close(pair[0]);
    close(pair[1]);
    throw std::runtime_error("Failed to fork shard worker");
  }
  if (process == 0)
  {
    close(pair[0]);
    fcntl(pair[1], F_SETFD, 0);
    std::vector<std::string> args{
        config.workerExecutable.string(), "--ipc-fd", std::to_string(pair[1]),
        "--shard-id", id.to_string()};
    for (const auto& module : config.modules)
    {
      args.emplace_back("--module");
      args.emplace_back(module);
    }
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    execv(argv.front(), argv.data());
    _exit(127);
  }
  close(pair[1]);
  rpc.AddWorker(id, pair[0]);
  shards.emplace(id, ShardInstance{.id = id, .region = RegionID(0),
                                   .process = process});
  return id;
}
void AtlasNet::ShardManager::DestroyShard(ShardID id)
{
  const auto found = shards.find(id);
  if (found == shards.end()) return;
  const pid_t process = found->second.process;
  kill(process, SIGTERM);
  bool reaped = false;
  for (int i = 0; i < 100; ++i)
  {
    if (waitpid(process, nullptr, WNOHANG) == process)
    {
      reaped = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if (!reaped)
  {
    kill(process, SIGKILL);
    waitpid(process, nullptr, 0);
  }
  rpc.RemoveWorker(id);
  shards.erase(found);
}
void AtlasNet::ShardManager::Poll()
{
  rpc.Poll(Network::PollType::NonBlocking);
  for (auto it = shards.begin(); it != shards.end();)
  {
    const pid_t result = waitpid(it->second.process, nullptr, WNOHANG);
    if (result == it->second.process || (result < 0 && errno == ECHILD))
    {
      rpc.RemoveWorker(it->first);
      it = shards.erase(it);
    }
    else
      ++it;
  }
}
bool AtlasNet::ShardManager::IsRunning(ShardID id) const
{
  const auto found = shards.find(id);
  return found != shards.end() && kill(found->second.process, 0) == 0;
}
void AtlasNet::ShardManager::Shutdown()
{
  while (!shards.empty()) DestroyShard(shards.begin()->first);
}
