#pragma once
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
namespace AtlasNet::Network
{
class INetworkInterface
{
public:
  virtual ~INetworkInterface() = default;
  virtual bool HasMACAddress() const = 0;
  virtual bool HasHostAddress() const = 0;
  virtual Network::MACAddress GetMACAddress() const = 0;
  virtual Network::HostAddress GetHostAddress() const = 0;
  virtual std::string GetName() const = 0;
};
} // namespace AtlasNet::Network