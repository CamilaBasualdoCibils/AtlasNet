#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
namespace AtlasNet::Network
{
struct DatagramBuffer
{
  SocketAddress source;

  std::span<const std::byte> data;

  void* owner = nullptr;
  void* userdata = nullptr;
  void (*release)(void*,void*) = nullptr;
  void Release()
  {
    if (release)
      release(owner, userdata);
  }

  
};
} // namespace AtlasNet::Network