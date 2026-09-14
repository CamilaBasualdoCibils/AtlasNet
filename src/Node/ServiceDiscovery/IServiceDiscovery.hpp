#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Node/NodeData.hpp"
namespace AtlasNet::Service
{


class IServiceLease
{
public:
  virtual ~IServiceLease() = default;
};
class IServiceDiscovery
{
public:
  virtual ~IServiceDiscovery() = default;
   [[nodiscard]] virtual std::unique_ptr<IServiceLease>
  RegisterService(const NodeData& data) = 0;
  virtual std::vector<NodeData>
  DiscoverServices() = 0;
  void SetOnServiceRegisteredCallback(
      std::function<void(const NodeData&)> callback)
  {
    onServiceRegistered = callback;
  }
  void SetOnServiceUnregisteredCallback(
      std::function<void(const NodeData&)> callback)
  {
    onServiceUnregistered = callback;
  }

private:
  void _onServiceRegistered(const NodeData& data)
  {
    if (onServiceRegistered)
    {
      onServiceRegistered(data);
    }
  }
  void _onServiceUnregistered(const NodeData& data)
  {
    if (onServiceUnregistered)
    {
      onServiceUnregistered(data);
    }
  }
  std::function<void(const NodeData&)> onServiceRegistered;
  std::function<void(const NodeData&)> onServiceUnregistered;
};
}; // namespace AtlasNet::Service