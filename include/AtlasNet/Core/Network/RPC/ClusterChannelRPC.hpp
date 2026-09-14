#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ClusterMessage.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/IClusterChannel.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPC.hpp"
namespace AtlasNet::Network::RPC
{

class ClusterChannelRPC : public RPC<AtlasNetNodeID, AtlasNetNodeID>
{
public:
  struct Config
  {
    AtlasNetNodeID selfID;
    std::shared_ptr<Cluster::IClusterChannel> clusterChannel;
  };
  ClusterChannelRPC(std::string_view Name, const Config& c) : RPC(Name), config_(c)
  {
  }

protected:


  void _ImplSendPacket(const AtlasNetNodeID& target,
                       std::span<const std::byte> packetData) override
  {
    config_.clusterChannel->Send(target, packetData);
  }

  void _ImplPoll(PollType pollType) override
  {
    config_.clusterChannel->Tick();
    constexpr size_t receiveBufferSize = 32;
    std::array<Cluster::ClusterMessage, receiveBufferSize> receiveBuffer;
    while (true)
    {
      size_t receiveNum =
          pollType == PollType::Blocking
              ? config_.clusterChannel->Receive(receiveBuffer)
              : config_.clusterChannel->TryReceive(receiveBuffer);
      for (size_t i = 0; i < receiveNum; ++i)
      {
        AtlasNetNodeID caller = receiveBuffer[i].Source();
        HandleIncomingPacket(caller, config_.selfID,
                             receiveBuffer[i].Payload());
      }
      if (receiveNum < receiveBufferSize)
      {
        break;
      }
    }
  }

  std::string TargetToString(const AtlasNetNodeID& target) const override
  {
    return target.to_string();
  }
  std::string CallerToString(const AtlasNetNodeID& caller) const override
  {
    return caller.to_string();
  }
private:
  Config config_;
};
} // namespace AtlasNet::Network::RPC