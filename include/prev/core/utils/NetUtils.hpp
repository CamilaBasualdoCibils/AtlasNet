#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>

#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

namespace NetUtils
{
    static uint32_t IPv4StringToUint(const std::string& ip)
    {
        in_addr addr{};
        if (inet_pton(AF_INET, ip.c_str(), &addr) != 1)
        {
            throw std::runtime_error("Invalid IPv4 address: " + ip);
        }

        return ntohl(addr.s_addr);
    }

    static bool ParseCIDR(const std::string& cidr, uint32_t& network, uint32_t& mask)
    {
        const std::size_t slash = cidr.find('/');
        if (slash == std::string::npos)
        {
            return false;
        }

        const std::string ipPart = cidr.substr(0, slash);
        const std::string prefixPart = cidr.substr(slash + 1);

        int prefixLength = 0;
        try
        {
            prefixLength = std::stoi(prefixPart);
        }
        catch (...)
        {
            return false;
        }

        if (prefixLength < 0 || prefixLength > 32)
        {
            return false;
        }

        try
        {
            network = IPv4StringToUint(ipPart);
        }
        catch (...)
        {
            return false;
        }

        if (prefixLength == 0)
        {
            mask = 0;
        }
        else
        {
            mask = 0xFFFFFFFFu << (32 - prefixLength);
        }

        network &= mask;
        return true;
    }

    std::optional<std::string> FindInterfaceIPInSubnet(const std::string& cidr)
    {
        uint32_t targetNetwork = 0;
        uint32_t targetMask = 0;

        if (!ParseCIDR(cidr, targetNetwork, targetMask))
        {
            throw std::runtime_error("Invalid CIDR subnet: " + cidr);
        }

        ifaddrs* interfaces = nullptr;
        if (getifaddrs(&interfaces) != 0)
        {
            throw std::runtime_error("getifaddrs() failed");
        }

        std::optional<std::string> result;

        for (ifaddrs* iface = interfaces; iface != nullptr; iface = iface->ifa_next)
        {
            if (iface->ifa_addr == nullptr)
            {
                continue;
            }

            if (iface->ifa_addr->sa_family != AF_INET)
            {
                continue;
            }

            const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
            const uint32_t ip = ntohl(addr->sin_addr.s_addr);

            if ((ip & targetMask) == targetNetwork)
            {
                char ipStr[INET_ADDRSTRLEN] = {};
                if (inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr)) != nullptr)
                {
                    result = std::string(ipStr);
                    break;
                }
            }
        }

        freeifaddrs(interfaces);
        return result;
    }
}