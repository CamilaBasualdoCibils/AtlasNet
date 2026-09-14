#include "AtlasNet/Core/Network/Transport/Datagram/UDP/UDPTransport.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/NetworkPacket.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
size_t
AtlasNet::Network::UDPTransport::Receive(std::span<DatagramBuffer> packets)
{
  {
    if (!socket_.has_value())
    {
      throw std::runtime_error("UDPTransport socket not initialized");
    }
    while (true)
    {
      auto count = TryReceive(packets);

      if (count > 0)
        return count;

      pollfd pfd{};
      pfd.fd = socket_.value();
      pfd.events = POLLIN;

      poll(&pfd, 1, -1);
    }
  }
}
size_t
AtlasNet::Network::UDPTransport::TryReceive(std::span<DatagramBuffer> datagrams)
{
  size_t received = 0;
  if (!socket_.has_value())
  {
    throw std::runtime_error("UDPTransport socket not initialized");
  }
  while (received < datagrams.size())
  {

    DatagramBuffer& datagram = datagrams[received];

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    UDPBuffer* buffer = GetFreeBuffer();
    buffer->data.resize(buffer->data.static_capacity);
    ssize_t bytes = recvfrom(socket_.value(), buffer->data.data(),
                             buffer->data.static_capacity, MSG_DONTWAIT,
                             reinterpret_cast<sockaddr*>(&addr), &addrLen);
    buffer->data.resize(bytes > 0 ? bytes : 0);
    if (bytes < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      logger->error("recvfrom failed: {}", strerror(errno));
      break;
    }
    datagram.source = SocketAddress(reinterpret_cast<sockaddr*>(&addr));
    datagram.data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(buffer->data.data()), bytes);
    datagram.owner = this;
    datagram.userdata = buffer;
    datagram.release = [](void* owner, void* userdata)
    {
      auto* buffer = static_cast<UDPBuffer*>(userdata);
      auto* transport = static_cast<UDPTransport*>(owner);
      transport->ReleaseBuffer(buffer);
    };
    if (addr.ss_family == AF_INET)
    {
      auto* a = reinterpret_cast<sockaddr_in*>(&addr);

      // packet.sourceAddress =
      //     SocketAddress(IPv4(ntohl(a->sin_addr.s_addr)), ntohs(a->sin_port));
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
bool AtlasNet::Network::UDPTransport::Listen(const SocketAddress& address)
{

  //
  // Create socket matching the address family
  //
  int family = address.IsIPv6() ? AF_INET6 : AF_INET;

  socket_ = socket(family, SOCK_DGRAM, 0);

  if (!socket_.has_value() || socket_.value() < 0)
  {
    throw std::runtime_error("Failed creating UDP socket");
  }

  //
  // Allow IPv4 mapped addresses on IPv6 sockets
  // (only applies to IPv6 sockets)
  //
  if (family == AF_INET6)
  {
    int off = 0;
    setsockopt(socket_.value(), IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
  }

  sockaddr_storage addr{};
  socklen_t addrLength = 0;

  if (address.IsIPv6())
  {
    sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(&addr);

    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = htons(address.get_port());

    const auto bytes = address.get_ipv6().get_bytes();

    std::memcpy(&addr6->sin6_addr, bytes.data(), bytes.size());

    addrLength = sizeof(sockaddr_in6);
  }
  else
  {
    sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&addr);

    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(address.get_port());

    addr4->sin_addr.s_addr = htonl(address.get_ipv4().to_uint32());

    addrLength = sizeof(sockaddr_in);
  }

  if (bind(socket_.value(), reinterpret_cast<sockaddr*>(&addr), addrLength) < 0)
  {
    logger->error("Failed to bind UDP socket on {}:{}, error: {}",
                  address.IsIPv4() ? address.get_ipv4().to_string()
                                   : address.get_ipv6().to_string(),
                  address.get_port(), strerror(errno));

    close(socket_.value());
    socket_ = std::nullopt;

    throw std::runtime_error("Failed to bind UDP socket");
  }

  logger->info("UDPTransport listening on {}:{}",
               address.IsIPv4() ? address.get_ipv4().to_string()
                                : address.get_ipv6().to_string(),
               address.get_port());

  //
  // Make socket non-blocking
  //
  int flags = fcntl(socket_.value(), F_GETFL, 0);

  if (flags >= 0)
  {
    fcntl(socket_.value(), F_SETFL, flags | O_NONBLOCK);
  }
  return true;
}

AtlasNet::Network::UDPTransport::~UDPTransport()
{
  if (socket_.has_value())
  {
    close(socket_.value());
  }
}
void AtlasNet::Network::UDPTransport::SendMessage(
    const SocketAddress& address, const PacketPayloadView& packet)
{

  sockaddr_storage addr{};
  socklen_t addrLen;

  if (address.IsIPv4())
  {
    auto* a = reinterpret_cast<sockaddr_in*>(&addr);

    a->sin_family = AF_INET;
    a->sin_port = htons(address.get_port());
    a->sin_addr.s_addr = htonl(address.get_ipv4().to_uint32());

    addrLen = sizeof(sockaddr_in);
  }
  else
  {
    auto* a = reinterpret_cast<sockaddr_in6*>(&addr);

    a->sin6_family = AF_INET6;
    a->sin6_port = htons(address.get_port());

    auto bytes = address.get_ipv6().get_bytes();
    std::memcpy(&a->sin6_addr, bytes.data(), 16);

    addrLen = sizeof(sockaddr_in6);
  }

  ssize_t result = sendto(socket_.value(), packet.data(), packet.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), addrLen);
  logger->info("Sent UDP message to {}:{} of {} bytes with result {}",
               address.to_string(), address.get_port(), packet.size(), result);

  if (result < 0)
  {
    logger->error("sendto failed: {}", strerror(errno));
  }
}