#include "AtlasNet/Core/Network/RPC/LocalShardRPC.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include "AtlasNet/Node/Module/ModuleAPI.hpp"
#include <memory>
#include <string>

namespace
{
using Ping =
    AtlasNet::Network::RPC::RPCMethod<"DummyShard.Ping", std::string>;
using GetShardID =
    AtlasNet::Network::RPC::RPCMethod<"DummyShard.GetShardID",
                                      AtlasNet::ShardID>;

class DummyShardLogic final : public AtlasNet::Module::IShardLogic
{
public:
  void Start(AtlasNet::ShardID id,
             AtlasNet::Network::RPC::LocalShardRPC& rpc) override
  {
    shardID = id;
    rpc.Bind<Ping>(
        [](const AtlasNet::Network::RPC::LocalShardRPC::CallContext&)
        { return std::string("pong"); });
    rpc.Bind<GetShardID>(
        [this](const AtlasNet::Network::RPC::LocalShardRPC::CallContext&)
        { return shardID; });
  }

  void Poll() override {}
  void Shutdown() override {}

private:
  AtlasNet::ShardID shardID{};
};

class DummyShardProvider final : public AtlasNet::Module::ShardProvider
{
public:
  std::unique_ptr<AtlasNet::Module::IShardLogic> CreateShardLogic() override
  {
    return std::make_unique<DummyShardLogic>();
  }
};

void Register(AtlasNet::Module::ModuleRegistry& registry)
{
  registry.RegisterShardProvider(std::make_shared<DummyShardProvider>());
}

const AtlasNet::Module::ModuleDescriptor descriptor{
    .name = "DummyShard",
    .version = "1.0.0",
    .capabilities = AtlasNet::Module::Capability::ShardProvider,
    .Register = Register};
} // namespace

extern "C" const AtlasNet::Module::ModuleDescriptor* AtlasNetGetModule()
{
  return &descriptor;
}
