#pragma once
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/RPC/LocalShardRPC.hpp"
#include <filesystem>
#include <sys/types.h>
#include <unordered_map>
#include <vector>
namespace AtlasNet
{
class ShardManager
{
public:
  struct Config
  {
    std::filesystem::path workerExecutable;
    std::vector<std::string> modules;
  };
  struct ShardInstance
  {
    ShardID id;
    RegionID region{0};
    pid_t process = -1;
  };
  explicit ShardManager(Config);
  ~ShardManager();
  ShardManager(const ShardManager&) = delete;
  ShardManager& operator=(const ShardManager&) = delete;
  ShardID CreateShard();
  void DestroyShard(ShardID);
  void Poll();
  void Shutdown();
  [[nodiscard]] bool IsRunning(ShardID) const;
  [[nodiscard]] std::size_t Size() const noexcept { return shards.size(); }
  Network::RPC::LocalShardRPC& RPC() noexcept { return rpc; }
private:
  Config config;
  uint32_t nextShardID = 0;
  std::unordered_map<ShardID, ShardInstance> shards;
  Network::RPC::LocalShardRPC rpc;
};
} // namespace AtlasNet
