#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterDatagram.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPC.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
namespace AtlasNet::Network::RPC
{
class NetworkTransportRPC : public RPC<SocketAddress, SocketAddress>
{
public:
  struct Config
  {
    std::shared_ptr<INetworkTransport> networkTransport;
  };
  NetworkTransportRPC(std::string_view Name, const Config& c)
      : RPC(Name), config_(c)
  {
  }

protected:
  void _ImplSendPacket(const SocketAddress& target,
                       std::span<const std::byte> packetData) override
  {
    config_.networkTransport->Send(target, packetData);
  }

  void _ImplPoll(PollType pollType) override
  {
    constexpr size_t receiveBufferSize = 32;
    std::array<TransportDatagram, receiveBufferSize> receiveBuffer;
    while (true)
    {
      size_t receiveNum =
          pollType == PollType::Blocking
              ? config_.networkTransport->Receive(receiveBuffer)
              : config_.networkTransport->TryReceive(receiveBuffer);
      for (size_t i = 0; i < receiveNum; ++i)
      {
        SocketAddress caller = receiveBuffer[i].source;

        HandleIncomingPacket(caller, caller, receiveBuffer[i].payload);
      }
      if (receiveNum < receiveBufferSize)
      {
        break;
      }
    }
  }

  std::string TargetToString(const SocketAddress& target) const override
  {
    return target.to_string();
  }
  std::string CallerToString(const SocketAddress& caller) const override
  {
    return caller.to_string();
  }

private:
  Config config_;
};
} // namespace AtlasNet::Network::RPC