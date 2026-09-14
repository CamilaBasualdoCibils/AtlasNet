#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/NetworkPacket.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/ConnectionCommons.hpp"
#include "AtlasNet/Core/Network/Transport/DatagramBuffer.hpp"
#include "AtlasNet/Core/Network/Transport/TransportCommons.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <emmintrin.h>
#include <functional>
#include <span>
namespace AtlasNet::Network
{
class IConnectionListener;
class IConnectionTransport;
class IConnection
{
  std::atomic<PacketID> nextPacketID{0};
  using StateChangeCallback = std::move_only_function<void(ConnectionState)>;
  StateChangeCallback stateChangeCallback;
  const SocketAddress remoteAddress;
  ConnectionState state{ConnectionState::None};

public:
  IConnection(SocketAddress address) : remoteAddress(address) {}
  PacketID Send(PacketPayloadView data, PacketSendMode mode)
  {
    PacketID packetID = nextPacketID.fetch_add(1, std::memory_order_relaxed);
    _SendPacket(packetID, data, mode);
    return packetID;
  }

  virtual ~IConnection() = default;

  virtual size_t Receive(std::span<DatagramBuffer> packets) = 0;

  virtual size_t TryReceive(std::span<DatagramBuffer> packets) = 0;
  virtual void Disconnect() = 0;

  virtual SocketAddress RemoteAddress() const {
    return remoteAddress;
  }
  virtual ConnectionState GetState() const
  {
    return state;
  }

  virtual void OnStateChange(StateChangeCallback callback)
  {
    stateChangeCallback = std::move(callback);
  }
  virtual bool WaitForState(ConnectionState desiredState, std::chrono::milliseconds timeout)
  {
    auto start = std::chrono::steady_clock::now();
    while (GetState() != desiredState)
    {
      if (std::chrono::steady_clock::now() - start > timeout)
      {
        return false;
      }
      _mm_pause(); // Yield to other threads
    }
    return true;
  }

protected:
  void _NotifyStateChange(ConnectionState newState)
  {
    state = newState;
    if (stateChangeCallback)
    {
      stateChangeCallback(state);
    }
  }

private:
  virtual void _SendPacket(PacketID packetID, PacketPayloadView data,
                           PacketSendMode mode) = 0;
};
}; // namespace AtlasNet::Network