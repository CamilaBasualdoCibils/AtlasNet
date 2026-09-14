#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <future>
namespace AtlasNet::Network::Topology
{
class ITopologyDeployer
{
public:
  virtual ~ITopologyDeployer() = default;

  virtual std::future<bool> Connect(AtlasNetNodeID a, AtlasNetNodeID b) = 0;
  virtual std::future<bool> Disconnect(AtlasNetNodeID a, AtlasNetNodeID b) = 0;
};
} // namespace AtlasNet::Network::Topology