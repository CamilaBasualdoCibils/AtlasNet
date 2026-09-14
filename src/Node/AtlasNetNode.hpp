#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Service/AtlasNetService.hpp"
#include <sw/redis++/async_redis.h>
#include <sys/signalfd.h>

namespace AtlasNet
{
class AtlasNetNode : public AtlasNetService
{
public:
  struct NodeConfig
  {
    struct IngressSocketOption
    {
      Network::Ingress::IngressTransportType type;
      uint16_t port;
      std::string ExtraArgs;
    };
    std::vector<IngressSocketOption> ingressSockets;
    Network::SocketAddress dbHandshakeAddress;
  };

  AtlasNetNode(int argc, char** argv)
      : AtlasNetService(AtlasNetServiceType::Node, argc, argv)
  {
  }

private:
  void ParseOptions(const boost::program_options::variables_map& vm) override;
  void AddOptions(boost::program_options::options_description& desc) override;
  void Initialize() override;

  void Tick() override {}

  NodeConfig nodeConfig;



};
} // namespace AtlasNet