#pragma once

#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Interface/INetworkInterface.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <linux/if.h>
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace AtlasNet::Network
{
class LinuxNetworkInterface : public INetworkInterface
{
  const std::string interface;
  std::optional<MACAddress> macAddress;
  std::optional<HostAddress> hostAddress;
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("LinuxNetworkInterface");

public:
  bool HasMACAddress() const override
  {
    return macAddress.has_value();
  }
  bool HasHostAddress() const override
  {
    return hostAddress.has_value();
  }
  Network::HostAddress GetHostAddress() const override
  {
    return hostAddress.value();
  }
  MACAddress GetMACAddress() const override
  {
    return macAddress.value();
  }
  std::string GetName() const override
  {
    return interface;
  }

  explicit LinuxNetworkInterface(const std::string& interface)
      : interface(interface)
  {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
      throw std::runtime_error("Failed creating socket");

    // MAC address
    ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
      close(fd);
      throw std::runtime_error("Failed to get MAC address");
    }

    std::array<uint8_t, 6> macBytes;
    std::memcpy(macBytes.data(), ifr.ifr_hwaddr.sa_data, 6);

    macAddress = MACAddress(macBytes);

    close(fd);

    // IP addresses
    ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) != 0)
      throw std::runtime_error("Failed to enumerate interfaces");

    for (auto* addr = interfaces; addr != nullptr; addr = addr->ifa_next)
    {
      if (!addr->ifa_addr)
        continue;

      if (interface != addr->ifa_name)
        continue;

      if (addr->ifa_addr->sa_family == AF_INET)
      {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr->ifa_addr);

        std::array<uint8_t, 4> bytes;
        std::memcpy(bytes.data(), &ipv4->sin_addr, 4);

        hostAddress = HostAddress(IPv4(bytes[0], bytes[1], bytes[2], bytes[3]));
        break;
      }
      else if (addr->ifa_addr->sa_family == AF_INET6)
      {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr->ifa_addr);

        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &ipv6->sin6_addr, 16);

        hostAddress = HostAddress(
            IPv6(bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
                 bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
                 bytes[12], bytes[13], bytes[14], bytes[15]));
        break;
      }
      else if (addr->ifa_addr->sa_family == AF_PACKET)
      {
        // do nothing since its just MAC address
      }
    }

    freeifaddrs(interfaces);
    logger->info("LinuxNetworkInterface initialized for interface: {}, MAC: "
                 "{}, HostAddress: {}",
                 interface,
                 macAddress.has_value() ? macAddress.value().ToString() : "N/A",
                 hostAddress.has_value() ? hostAddress.value().to_string()
                                         : "N/A");
  }
};
}; // namespace AtlasNet::Network