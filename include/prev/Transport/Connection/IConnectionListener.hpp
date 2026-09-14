#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include <functional>
#include <memory>
namespace AtlasNet::Network
{
class IConnection;
class ConnectionRequest;
class IConnectionListener
{
  friend class ConnectionRequest;

protected:
public:
  virtual ~IConnectionListener() = default;

  virtual void SetConnectionRequestCallback(
      std::function<void(ConnectionRequest&, IConnectionListener&)>
          callback) = 0;
  virtual void Close() = 0;
};

class ConnectionRequest
{
  SocketAddress remoteAddress;
  std::optional<bool> accepted;
  IConnectionListener* listener = nullptr;
  std::function<std::shared_ptr<IConnection>()> acceptCallback;
  std::function<void()> rejectCallback;

public:
  ConnectionRequest() = default;
  ConnectionRequest(
      IConnectionListener* listener, const SocketAddress& address,
      std::function<std::shared_ptr<IConnection>()> acceptCallback,
      std::function<void()> rejectCallback)
      : remoteAddress(address), listener(listener),
        acceptCallback(std::move(acceptCallback)),
        rejectCallback(std::move(rejectCallback))
  {
  }
  const SocketAddress& RemoteAddress() const
  {
    return remoteAddress;
  }
  [[nodiscard]] std::shared_ptr<IConnection> Accept()
  {
    if (accepted.has_value())
    {
      throw std::runtime_error(
          "ConnectionRequest already handled (accepted or rejected)");
    }
    accepted = true;
    return acceptCallback();
  }
  void Reject()
  {
    if (accepted.has_value())
    {
      throw std::runtime_error(
          "ConnectionRequest already handled (accepted or rejected)");
    }
    accepted = false;
    rejectCallback();
  }
  bool IsAccepted() const
  {

    return accepted.has_value() && accepted.value();
  }
  bool WasHandled() const
  {
    return accepted.has_value();
  }
};
} // namespace AtlasNet::Network