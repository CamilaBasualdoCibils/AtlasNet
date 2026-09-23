#include "AtlasNet/Core/Network/RPC/LocalShardRPC.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include "AtlasNet/Node/Module/ModuleAPI.hpp"
#include <csignal>
namespace
{
using Echo = AtlasNet::Network::RPC::RPCMethod<"Test.Echo", std::string, std::string>;
using Ready = AtlasNet::Network::RPC::RPCMethod<"Test.Ready", void, AtlasNet::ShardID>;
using Crash = AtlasNet::Network::RPC::RPCMethod<"Test.Crash", void>;
class Logic final : public AtlasNet::Module::IShardLogic
{
public:
  void Start(AtlasNet::ShardID id,
             AtlasNet::Network::RPC::LocalShardRPC& value) override
  {
    rpc = &value;
    rpc->Bind<Echo>([](const auto&, const std::string& text) { return text; });
    rpc->Bind<Crash>([](const auto&) { raise(SIGABRT); });
    rpc->Call<Ready>(id, id);
  }
  void Poll() override {}
  void Shutdown() override {}
private:
  AtlasNet::Network::RPC::LocalShardRPC* rpc = nullptr;
};
class Provider final : public AtlasNet::Module::ShardProvider
{
public:
  std::unique_ptr<AtlasNet::Module::IShardLogic> CreateShardLogic() override
  {
    return std::make_unique<Logic>();
  }
};
void Register(AtlasNet::Module::ModuleRegistry& registry)
{
  registry.RegisterShardProvider(std::make_shared<Provider>());
}
const AtlasNet::Module::ModuleDescriptor descriptor{
    .name = "TestShard", .version = "1", .capabilities = AtlasNet::Module::Capability::ShardProvider,
    .Register = Register};
}
extern "C" const AtlasNet::Module::ModuleDescriptor* AtlasNetGetModule()
{
  return &descriptor;
}
