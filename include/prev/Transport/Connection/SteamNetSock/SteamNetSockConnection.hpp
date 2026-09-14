#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/NetworkPacket.hpp"
#include "AtlasNet/Core/Network/Transport/Connection/IConnection.hpp"
#include "AtlasNet/Core/Network/Transport/DatagramBuffer.hpp"
#include "spdlog/logger.h"
#include <algorithm>
#include <atomic>
#include <emmintrin.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamclientpublic.h>
#include <steam/steamnetworkingtypes.h>

namespace AtlasNet::Network
{

class SteamNetSockTransport;
class SteamNetSockListener;
class SteamNetSockConnection;

class SteamNetSockConnection : public IConnection
{
  friend class SteamNetSockTransport;

  SteamNetSockTransport* steamNetworkingSockets;
  HSteamNetConnection connectionHandle;

public:
  SteamNetSockConnection(SteamNetSockTransport* transport,
                         HSteamNetConnection handle, SocketAddress address)
      : IConnection(address), steamNetworkingSockets(transport),
        connectionHandle(handle)
  {
  }
  size_t Receive(std::span<DatagramBuffer> packets) override
  {
    if (packets.empty())
      return 0;

    std::array<SteamNetworkingMessage_t*, 128> messages;

    const int maxMessages =
        static_cast<int>(std::min(packets.size(), messages.size()));

    while (true)
    {
      const int receiveResult =
          SteamNetworkingSockets()->ReceiveMessagesOnConnection(
              connectionHandle, messages.data(), maxMessages);

      assert(receiveResult >= 0);

      if (receiveResult > 0)
      {
        for (int i = 0; i < receiveResult; ++i)
        {
          SteamNetworkingMessage_t* msg = messages[i];
          DatagramBuffer& packet = packets[i];
          packet.owner = msg;
          packet.userdata = nullptr;
          packet.release = [](void* owner, void* userdata)
          {
            auto* msg = static_cast<SteamNetworkingMessage_t*>(owner);
            msg->Release();
          };
          packet.data = std::span<const std::byte>(
              static_cast<const std::byte*>(msg->m_pData), msg->m_cbSize);
          packet.source = RemoteAddress();
        }

        return static_cast<size_t>(receiveResult);
      }

      // No packets yet. Since Receive() is blocking by contract,
      // wait efficiently instead of burning the CPU.
      _mm_pause();
    }
  }

  size_t TryReceive(std::span<DatagramBuffer> packets) override
  {
    if (packets.empty())
      return 0;
    std::array<SteamNetworkingMessage_t*, 128> messages;

    const int maxMessages =
        static_cast<int>(std::min(packets.size(), messages.size()));
    const int receiveResult =
        SteamNetworkingSockets()->ReceiveMessagesOnConnection(
            connectionHandle, messages.data(), maxMessages);

    assert(receiveResult >= 0);

    if (receiveResult > 0)
    {
      for (int i = 0; i < receiveResult; ++i)
      {
        SteamNetworkingMessage_t* msg = messages[i];

        DatagramBuffer& packet = packets[i];
        packet.owner = msg;
        packet.userdata = nullptr;
        packet.release = [](void* owner, void* userdata)
        {
          auto* msg = static_cast<SteamNetworkingMessage_t*>(owner);
          msg->Release();
        };
        packet.data = std::span<const std::byte>(
            static_cast<const std::byte*>(msg->m_pData), msg->m_cbSize);
        packet.source = RemoteAddress();
      }

      return static_cast<size_t>(receiveResult);
    }

    return 0;
  }

  void Disconnect() override
  {
    SteamNetworkingSockets()->CloseConnection(connectionHandle, 0, "Disconnect",
                                              true);
  }

private:
  void _SendPacket(PacketID packetID, PacketPayloadView data,
                   PacketSendMode mode) override
  {
    int sendFlags = 0;
    switch (mode)
    {
    case PacketSendMode::Reliable:
      sendFlags = k_nSteamNetworkingSend_ReliableNoNagle;
      break;
    case PacketSendMode::ReliableBatched:
      sendFlags = k_nSteamNetworkingSend_Reliable;
      break;
    case PacketSendMode::Unreliable:
      sendFlags = k_nSteamNetworkingSend_UnreliableNoNagle;
      break;
    case PacketSendMode::UnreliableBatched:
      sendFlags = k_nSteamNetworkingSend_Unreliable;
      break;
    case PacketSendMode::NoDelay:
      sendFlags = k_nSteamNetworkingSend_UnreliableNoDelay;
      break;
    default:
      assert(false && "Invalid PacketSendMode");
      return;
    }

    int result = SteamNetworkingSockets()->SendMessageToConnection(
        connectionHandle, data.data(), data.size(), sendFlags, nullptr);
    assert(result != k_EResultInvalidState && result != k_EResultInvalidParam &&
           result != k_EResultNoConnection && result != k_EResultLimitExceeded);
  }
};
} // namespace AtlasNet::Network