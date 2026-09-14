#pragma once

#include "AtlasNet/Core/Core.hpp"
namespace AtlasNet::Network::Topology
{
class ITopologyAssigner
{
public:
  virtual ~ITopologyAssigner() = default;
  void OnConnectionAssign(
      std::function<void(AtlasNetNodeID, AtlasNetNodeID)> callback)
  {
    connectionAssignCallback = callback;
  }
  void OnDisconnectionAssign(
      std::function<void(AtlasNetNodeID, AtlasNetNodeID)> callback)
  {
    disconnectionAssignCallback = callback;
  }
  protected:
  void TriggerConnectionAssign(AtlasNetNodeID a, AtlasNetNodeID b)
  {
    if (connectionAssignCallback)
    {
      connectionAssignCallback(a, b);
    }
  }
  void TriggerDisconnectionAssign(AtlasNetNodeID a, AtlasNetNodeID b)
  {
    if (disconnectionAssignCallback)
    {
      disconnectionAssignCallback(a, b);
    }
  }

private:
  std::function<void(AtlasNetNodeID, AtlasNetNodeID)> connectionAssignCallback,
      disconnectionAssignCallback;
};
} // namespace AtlasNet::Network::Topology