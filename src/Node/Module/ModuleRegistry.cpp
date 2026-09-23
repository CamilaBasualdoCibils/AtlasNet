#include "AtlasNet/Node/Module/ModuleRegistry.hpp"
#include <stdexcept>
void AtlasNet::Module::ModuleRegistry::RegisterShardProvider(
    std::shared_ptr<ShardProvider> provider)
{
  if (!provider)
    throw std::invalid_argument("Cannot register a null shard provider");
  if (shardProvider)
    throw std::runtime_error(
        "Multiple modules attempted to register a ShardProvider");
  shardProvider = std::move(provider);
}
void AtlasNet::Module::ModuleRegistry::RegisterClientIngressTransport(
    std::string_view name,
    std::shared_ptr<ClientIngressTransportProvider> provider)
{
  if (name.empty())
    throw std::invalid_argument(
        "Client ingress transport name cannot be empty");
  if (!provider)
    throw std::invalid_argument(
        "Cannot register a null client ingress transport provider");
  const auto [_, inserted] =
      ingressTransports.emplace(std::string(name), std::move(provider));
  if (!inserted)
    throw std::runtime_error("Duplicate client ingress transport: " +
                             std::string(name));
}
bool AtlasNet::Module::ModuleRegistry::HasShardProvider() const noexcept
{
  return static_cast<bool>(shardProvider);
}
std::shared_ptr<AtlasNet::Module::ShardProvider>
AtlasNet::Module::ModuleRegistry::GetShardProvider() const
{
  return shardProvider;
}
std::shared_ptr<AtlasNet::Module::ClientIngressTransportProvider>
AtlasNet::Module::ModuleRegistry::GetClientIngressTransport(
    std::string_view name) const
{
  const auto found = ingressTransports.find(std::string(name));
  return found == ingressTransports.end() ? nullptr : found->second;
}
uint64_t
AtlasNet::Module::ModuleRegistry::RegisteredCapabilities() const noexcept
{
  return (shardProvider ? 1ULL : 0ULL) |
         (ingressTransports.empty() ? 0ULL : 2ULL);
}
void AtlasNet::Module::ModuleRegistry::MergeFrom(ModuleRegistry&& other)
{
  if (shardProvider && other.shardProvider)
    throw std::runtime_error(
        "Multiple modules attempted to register a ShardProvider");
  for (const auto& [name, _] : other.ingressTransports)
    if (ingressTransports.contains(name))
      throw std::runtime_error("Duplicate client ingress transport: " + name);
  if (other.shardProvider)
    shardProvider = std::move(other.shardProvider);
  ingressTransports.merge(other.ingressTransports);
}
