#pragma once
#include "AtlasNet/Core/Types/ShardID.hpp"
#include <cstdint>
#include <memory>
#include <string>
namespace AtlasNet::Network::RPC
{
class LocalShardRPC;
}
namespace AtlasNet::Module
{
class IShardLogic
{
public:
  virtual ~IShardLogic() = default;
  virtual void Start(ShardID, Network::RPC::LocalShardRPC&) = 0;
  virtual void Poll() = 0;
  virtual void Shutdown() = 0;
};
class ShardProvider
{
public:
  virtual ~ShardProvider() = default;
  virtual std::unique_ptr<IShardLogic> CreateShardLogic() { return {}; }
};
struct ClientIngressListenerConfig
{
  uint16_t port = 0;
  std::string arguments;
};
class ClientIngressListener
{
public:
  virtual ~ClientIngressListener() = default;
  virtual void Start() = 0;
  virtual void Poll() = 0;
  virtual void Shutdown() = 0;
};
class ClientIngressTransportProvider
{
public:
  virtual ~ClientIngressTransportProvider() = default;
  virtual std::unique_ptr<ClientIngressListener>
  CreateListener(const ClientIngressListenerConfig& config) = 0;
};
} // namespace AtlasNet::Module
