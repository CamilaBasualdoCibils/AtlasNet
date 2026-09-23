#pragma once
#include "AtlasNet/Node/Module/Providers.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
namespace AtlasNet::Module
{
class ModuleRegistry
{
public:
  void RegisterShardProvider(std::shared_ptr<ShardProvider> provider);
  void RegisterClientIngressTransport(
      std::string_view name,
      std::shared_ptr<ClientIngressTransportProvider> provider);
  [[nodiscard]] bool HasShardProvider() const noexcept;
  [[nodiscard]] std::shared_ptr<ShardProvider> GetShardProvider() const;
  [[nodiscard]] std::shared_ptr<ClientIngressTransportProvider>
  GetClientIngressTransport(std::string_view name) const;
  [[nodiscard]] uint64_t RegisteredCapabilities() const noexcept;
  void MergeFrom(ModuleRegistry&& other);

private:
  std::shared_ptr<ShardProvider> shardProvider;
  std::unordered_map<std::string,
                     std::shared_ptr<ClientIngressTransportProvider>>
      ingressTransports;
};
} // namespace AtlasNet::Module
