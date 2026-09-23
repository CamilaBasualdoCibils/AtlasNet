#include "AtlasNet/Core/Network/RPC/LocalShardRPC.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
namespace
{
std::runtime_error SocketError(const char* op)
{
  return std::runtime_error(std::string(op) + ": " + std::strerror(errno));
}
}
AtlasNet::Network::RPC::LocalShardRPC::LocalShardRPC(std::string_view name)
    : RPC(name) {}
AtlasNet::Network::RPC::LocalShardRPC::LocalShardRPC(std::string_view name,
                                                     ShardID shard, int socket)
    : RPC(name), workerSide(true), workerShard(shard)
{
  if (socket < 0) throw std::invalid_argument("Invalid shard IPC socket");
  sockets.emplace(shard, socket);
}
AtlasNet::Network::RPC::LocalShardRPC::~LocalShardRPC()
{
  for (const auto& [_, socket] : sockets) close(socket);
}
void AtlasNet::Network::RPC::LocalShardRPC::AddWorker(ShardID shard, int socket)
{
  if (workerSide || socket < 0)
    throw std::invalid_argument("Invalid shard IPC connection");
  if (!sockets.emplace(shard, socket).second)
    throw std::logic_error("Shard IPC connection already exists");
}
void AtlasNet::Network::RPC::LocalShardRPC::RemoveWorker(ShardID shard)
{
  const auto found = sockets.find(shard);
  if (found == sockets.end()) return;
  close(found->second);
  sockets.erase(found);
}
bool AtlasNet::Network::RPC::LocalShardRPC::HasWorker(ShardID shard) const
{
  return sockets.contains(shard);
}
void AtlasNet::Network::RPC::LocalShardRPC::_ImplSendPacket(
    const ShardID& target, std::span<const std::byte> data)
{
  const auto found = sockets.find(workerSide ? workerShard : target);
  if (found == sockets.end()) throw std::runtime_error("No local shard worker");
  const auto sent = send(found->second, data.data(), data.size(), MSG_NOSIGNAL);
  if (sent < 0) throw SocketError("send shard RPC");
  if (static_cast<std::size_t>(sent) != data.size())
    throw std::runtime_error("Incomplete shard RPC packet send");
}
void AtlasNet::Network::RPC::LocalShardRPC::_ImplPoll(PollType pollType)
{
  std::array<std::byte, RPCHeader::MaxPayloadSize + 128> packet{};
  bool first = true;
  for (const auto& [shard, socket] : sockets)
    for (;;)
    {
      const int flags =
          (pollType == PollType::NonBlocking || !first) ? MSG_DONTWAIT : 0;
      first = false;
      const auto received = recv(socket, packet.data(), packet.size(), flags);
      if (received > 0)
      {
        HandleIncomingPacket(
            shard, shard,
            std::span(packet.data(), static_cast<std::size_t>(received)));
        continue;
      }
      if (received == 0 || errno == EAGAIN || errno == EWOULDBLOCK ||
          errno == EINTR)
        break;
      throw SocketError("receive shard RPC");
    }
}
std::string AtlasNet::Network::RPC::LocalShardRPC::CallerToString(
    const ShardID& id) const { return id.to_string(); }
std::string AtlasNet::Network::RPC::LocalShardRPC::TargetToString(
    const ShardID& id) const { return id.to_string(); }
