#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include <string>
namespace AtlasNet
{
   struct NodeData
{
  std::string hostID;
  Network::SocketAddress internalAddress;
  Network::MACAddress macAddress;
  AtlasNetNodeID nodeID;
  _Json to_json() const
  {
    return _Json{
        {"hostID", hostID},
        {"internalAddress", internalAddress.to_string()},
        {"macAddress", macAddress.ToString()},
        {"nodeID", nodeID.to_string()},
    };
  }
  void from_json(const _Json& j)
  {
    hostID = j.at("hostID").get<std::string>();
    internalAddress.parse_string(j.at("internalAddress").get<std::string>());
    macAddress =
        Network::MACAddress::FromString(j.at("macAddress").get<std::string>());
    nodeID = AtlasNetNodeID::from_string(j.at("nodeID").get<std::string>());
  }
};
};