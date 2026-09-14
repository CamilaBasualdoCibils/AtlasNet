#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Transport/ITransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportCommons.hpp"
#include <functional>
#include <memory>
#include <span>
namespace AtlasNet::Network
{
class IConnectionListener;
class IConnection;
class IConnectionTransport : public ITransport
{
    public:
  virtual ~IConnectionTransport() = default;

  [[nodiscard]] virtual std::shared_ptr<IConnectionListener>
  Listen(const SocketAddress&) = 0;

  [[nodiscard]] virtual std::shared_ptr<IConnection>
  Connect(const SocketAddress&) = 0;
};
}; // namespace AtlasNet::Network