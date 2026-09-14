#pragma once
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Intent/ClusterIntentChannel.hpp"
#include "AtlasNet/Core/Network/Intent/IntentRecepient.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPC.hpp"
#include "RPC.hpp"
namespace AtlasNet::Network::RPC
{
struct ClusterIntentRPCCaller
{
  Intent::VIntent intent;
  AtlasNetNodeID nodeID;
};
class ClusterIntentRPC : public RPC<Intent::VIntent, ClusterIntentRPCCaller>
{
public:
  struct Config
  {
    std::shared_ptr<Intent::ClusterIntentChannel> clusterIntentChannel;
  };
  ClusterIntentRPC(std::string_view name, const Config& c)
      : RPC(name), config_(c)
  {
  }

protected:
  void _ImplSendPacket(const Intent::VIntent& target,
                       std::span<const std::byte> packetData) override
  {
    config_.clusterIntentChannel->Send(target, packetData);
  }

  void _ImplPoll(PollType pollType) override
  {
    config_.clusterIntentChannel->Tick();
    constexpr size_t receiveBufferSize = 32;
    std::array<Intent::IntentDatagram, receiveBufferSize> receiveBuffer;
    while (true)
    {
      size_t receiveNum =
          pollType == PollType::Blocking
              ? config_.clusterIntentChannel->Receive(receiveBuffer)
              : config_.clusterIntentChannel->TryReceive(receiveBuffer);
      for (size_t i = 0; i < receiveNum; ++i)
      {
        ClusterIntentRPCCaller caller{receiveBuffer[i].Header().intent,
                                      receiveBuffer[i].Header().source};
        HandleIncomingPacket(caller, receiveBuffer[i].Payload());
      }
      if (receiveNum < receiveBufferSize)
      {
        break;
      }
    }
  }

  std::string TargetToString(const Intent::VIntent& target) const override
  {
    return Intent::IntentToString(target);
  }

private:
  Config config_;
};
} // namespace AtlasNet::Network::RPC