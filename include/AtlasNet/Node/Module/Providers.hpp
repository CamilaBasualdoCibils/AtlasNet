#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace AtlasNet::Module
{
class ShardProvider
{
public:
  virtual ~ShardProvider() = default;
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
