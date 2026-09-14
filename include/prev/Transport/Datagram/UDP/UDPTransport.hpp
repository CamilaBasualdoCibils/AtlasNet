#pragma once
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/NetworkPacket.hpp"
#include "AtlasNet/Core/Network/Transport/Datagram/IDatagramTransport.hpp"
#include <boost/container/static_vector.hpp>
#include <queue>
#include <unordered_set>

namespace AtlasNet::Network
{

class UDPTransport : public IDatagramTransport
{
  constexpr static size_t MaxUDPPacketSize = 65536;

  struct UDPBuffer
  {
    boost::container::static_vector<uint8_t, MaxUDPPacketSize> data;
  };

  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("UDPTransport");
  std::optional<int> socket_;
  std::unordered_set<std::unique_ptr<UDPBuffer>> storage;
  std::queue<UDPBuffer*> freeBuffers;

  UDPBuffer* GetFreeBuffer()
  {
    if (freeBuffers.empty())
    {
      auto buffer = std::make_unique<UDPBuffer>();
      auto ptr = buffer.get();
      storage.insert(std::move(buffer));
      return ptr;
    }
    else
    {
      auto ptr = freeBuffers.front();
      freeBuffers.pop();
      return ptr;
    }
  }
  void ReleaseBuffer(UDPBuffer* buffer)
  {
    freeBuffers.push(buffer);
  }
public:
  bool Listen(const SocketAddress& address) override;

  size_t Receive(std::span<DatagramBuffer> packets) override;

  size_t TryReceive(std::span<DatagramBuffer> packets) override;

  UDPTransport()
  {

    //
    // Receive thread
    //
  }
  ~UDPTransport();
  void SendMessage(const SocketAddress& address,
                   const PacketPayloadView& data) override;
};
}; // namespace AtlasNet::Network