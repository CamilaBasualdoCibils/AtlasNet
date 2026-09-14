#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
namespace AtlasNet::Network
{
struct TransportDatagram
{
  SocketAddress source;

  std::span<const std::byte> payload;

  void* owner = nullptr;
  void* userdata = nullptr;
  void (*release)(void*, void*) = nullptr;
  void Release()
  {
    if (release)
      release(owner, userdata);
  }
};
} // namespace AtlasNet::Network