#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Transport/INetworkTransport.hpp"
#include "AtlasNet/Core/Network/Transport/TransportDatagram.hpp"
#include <boost/container/static_vector.hpp>
#include <fcntl.h>
#include <memory>
#include <queue>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <sys/poll.h>
#include <unordered_set>
namespace AtlasNet::Network
{
class UDPNetworkTransport : public INetworkTransport
{
public:
  UDPNetworkTransport(std::string_view name, const SocketAddress& listenAddress)
      : logger(spdlog::stdout_color_mt(name.data())),
        listenAddress_(listenAddress)
  {
    socket_ = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_ < 0)
    {
      throw std::runtime_error("Failed to create UDP socket: " +
                               std::string(std::strerror(errno)));
    }

    // Allow IPv4 mapped addresses too
    int v6Only = 0;
    if (setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, &v6Only,
                   sizeof(v6Only)) < 0)
    {
      close(socket_);
      throw std::runtime_error("Failed to disable IPV6_V6ONLY");
    }

    // Allow fast restart after crashes
    // int reuse = 1;
    // setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;

    // Port 0 tells the OS to choose an ephemeral port.
    addr.sin6_port = htons(listenAddress.get_port() == PORT_EPHEMERAL
                               ? 0
                               : listenAddress.get_port());

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
      const auto error = std::string(std::strerror(errno));
      close(socket_);

      logger->error("Failed to bind UDP socket: {}", error);
      throw std::runtime_error("Failed to bind UDP socket: " + error);
    }
   /*  sockaddr_storage actual{};
    socklen_t actualLen = sizeof(actual);

    if (::getsockname(socket_, reinterpret_cast<sockaddr*>(&actual),
                      &actualLen) < 0)
    {
      logger->error("getsockname failed: {}", strerror(errno));
    }
    else
    {
      logger->info(
          "UDP fd {} actually bound to {}", socket_,
          SocketAddress(reinterpret_cast<sockaddr*>(&actual)).to_string());
    } */
    // If we requested an ephemeral port, find out which one
    // the OS actually assigned.
    if (listenAddress.get_port() == PORT_EPHEMERAL)
    {
      sockaddr_in6 boundAddr{};
      socklen_t boundAddrLen = sizeof(boundAddr);

      if (::getsockname(socket_, reinterpret_cast<sockaddr*>(&boundAddr),
                        &boundAddrLen) < 0)
      {
        const auto error = std::string(std::strerror(errno));
        close(socket_);

        throw std::runtime_error("Failed to get assigned UDP port: " + error);
      }
      PortType listenPort = ntohs(boundAddr.sin6_port);

      logger->debug("UDP socket assigned ephemeral port {}", listenPort);
      listenAddress_ =
          SocketAddress(listenAddress_.to_host_address(), listenPort);
    }

    // Non-blocking
    int flags = fcntl(socket_, F_GETFL, 0);

    if (flags < 0 || fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      const auto error = std::string(std::strerror(errno));
      close(socket_);

      throw std::runtime_error("Failed to set UDP socket non-blocking: " +
                               error);
    }
  }
  bool Send(const SocketAddress& destination,
            std::span<const std::byte> payload) override
  {
    sockaddr_storage addr{};
    socklen_t addrLen;
    SocketAddress resolvedDestination = destination.Resolve();
    if (!resolvedDestination.IsValid())
    {
      logger->error("Address is not valid: {}",
                    resolvedDestination.to_string());
      return false;
    }
    if (resolvedDestination.IsIPv4())
    {
      auto* a = reinterpret_cast<sockaddr_in6*>(&addr);

      a->sin6_family = AF_INET6;
      a->sin6_port = htons(resolvedDestination.get_port());

      // ::ffff:a.b.c.d
      a->sin6_addr = {};
      a->sin6_addr.s6_addr[10] = 0xff;
      a->sin6_addr.s6_addr[11] = 0xff;

      uint32_t ipv4 = htonl(resolvedDestination.get_ipv4().to_uint32());
      std::memcpy(&a->sin6_addr.s6_addr[12], &ipv4, sizeof(ipv4));

      addrLen = sizeof(sockaddr_in6);
    }
    else
    {
      auto* a = reinterpret_cast<sockaddr_in6*>(&addr);

      a->sin6_family = AF_INET6;
      a->sin6_port = htons(resolvedDestination.get_port());

      auto bytes = resolvedDestination.get_ipv6().get_bytes();
      std::memcpy(&a->sin6_addr, bytes.data(), 16);

      addrLen = sizeof(sockaddr_in6);
    }

    ssize_t result = sendto(socket_, payload.data(), payload.size(), 0,
                            reinterpret_cast<sockaddr*>(&addr), addrLen);
    logger->info("Sent UDP message to {} of {} bytes with result {}",
                 destination.to_string(), payload.size(), result);

    if (result < 0)
    {
      logger->error("sendto failed: {}", strerror(errno));
    }
    return true;
  }

  size_t Receive(std::span<TransportDatagram> packets) override
  {
    if (!socket_)
    {
      throw std::runtime_error("UDPTransport socket not initialized");
    }
    while (true)
    {
      auto count = TryReceive(packets);

      if (count > 0)
        return count;

      pollfd pfd{};
      pfd.fd = socket_;
      pfd.events = POLLIN;

      poll(&pfd, 1, -1);
    }
  }

  size_t TryReceive(std::span<TransportDatagram> packets) override
  {
    size_t received = 0;
    while (received < packets.size())
    {

      TransportDatagram& datagram = packets[received];

      sockaddr_storage addr{};
      socklen_t addrLen = sizeof(addr);

      UDPBuffer* buffer = GetFreeBuffer();
      buffer->data.resize(buffer->data.static_capacity);
      logger->trace("trying recvfrom on fd {}", socket_);
      ssize_t bytes =
          recvfrom(socket_, buffer->data.data(), buffer->data.static_capacity,
                   MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&addr), &addrLen);
      buffer->data.resize(bytes > 0 ? bytes : 0);
      if (bytes < 0)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          logger->trace("recvfrom: nothing available");
          break;
        }

        logger->error("recvfrom failed: {} ({})", strerror(errno), errno);
        break;
      }
      logger->debug("recvfrom: received {} bytes from fd {}", bytes, socket_);
      datagram.source = SocketAddress(reinterpret_cast<sockaddr*>(&addr));
      datagram.payload = std::span<const std::byte>(
          reinterpret_cast<const std::byte*>(buffer->data.data()), bytes);
      datagram.owner = this;
      datagram.userdata = buffer;
      datagram.release = [](void* owner, void* userdata)
      {
        auto* buffer = static_cast<UDPBuffer*>(userdata);
        auto* transport = static_cast<UDPNetworkTransport*>(owner);
        transport->ReleaseBuffer(buffer);
      };
      if (addr.ss_family == AF_INET)
      {
        auto* a = reinterpret_cast<sockaddr_in*>(&addr);

        // packet.sourceAddress =
        //     SocketAddress(IPv4(ntohl(a->sin_addr.s_addr)),
        //     ntohs(a->sin_port));
      }
      else
      {
        auto* a = reinterpret_cast<sockaddr_in6*>(&addr);

        std::array<uint8_t, 16> ip;
        std::memcpy(ip.data(), &a->sin6_addr, 16);

        // packet.sourceAddress =
        //     SocketAddress(IPv6(ip.data()), ntohs(a->sin6_port));
      }

      ++received;
    }

    return received;
  }

  virtual ~UDPNetworkTransport() = default;

  SocketAddress GetListenAddress() override
  {
    return listenAddress_;
  }
  PortType GetListenPort() override
  {
    return listenAddress_.get_port();
  }

private:
  constexpr static size_t MaxUDPPacketSize = 65536;
  struct UDPBuffer
  {
    boost::container::static_vector<uint8_t, MaxUDPPacketSize> data;
  };
  SocketAddress listenAddress_;
  std::unordered_set<std::unique_ptr<UDPBuffer>> storage;
  std::queue<UDPBuffer*> freeBuffers;
  int socket_;
  std::shared_ptr<spdlog::logger> logger;

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
};
} // namespace AtlasNet::Network