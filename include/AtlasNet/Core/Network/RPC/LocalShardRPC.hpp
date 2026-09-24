#pragma once
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/RPC/RPC.hpp"
#include <unordered_map>
namespace AtlasNet::Network::RPC
{
class LocalShardRPC final : public RPC<ShardID, ShardID>
{
public:
  explicit LocalShardRPC(std::string_view name);
  LocalShardRPC(std::string_view name, ShardID shard, int socket);
  ~LocalShardRPC();
  LocalShardRPC(const LocalShardRPC&) = delete;
  LocalShardRPC& operator=(const LocalShardRPC&) = delete;
  void AddWorker(ShardID shard, int socket);
  void RemoveWorker(ShardID shard);
  [[nodiscard]] bool HasWorker(ShardID shard) const;
protected:
  void _ImplSendPacket(const ShardID&, std::span<const std::byte>) override;
  void _ImplPoll(PollType) override;
  std::string CallerToString(const ShardID&) const override;
  std::string TargetToString(const ShardID&) const override;
private:
  bool workerSide = false;
  ShardID workerShard{};
  std::unordered_map<ShardID, int> sockets;
};
} // namespace AtlasNet::Network::RPC
