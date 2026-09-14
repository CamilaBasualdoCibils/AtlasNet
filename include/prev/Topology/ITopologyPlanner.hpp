#pragma once

#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
namespace AtlasNet::Network::Topology
{
class ITopologyPlanner
{
public:
  virtual ~ITopologyPlanner() = default;

  virtual ConnectionGraph Compute(const WeightGraph& nodes) = 0;
};
} // namespace AtlasNet::Network::Topology