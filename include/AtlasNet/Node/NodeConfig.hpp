#pragma once
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/ClusterCommons.hpp"
#include "AtlasNet/Core/Network/Ingress/IngressCommons.hpp"
#include "AtlasNet/Node/NodeCapability.hpp"
#include <string>
#include <vector>

namespace AtlasNet
{
struct NodeConfig
{
  struct TransportOptions
  {
    Network::Cluster::ClusterTransportType networkTransportType =
        Network::Cluster::ClusterTransportType::UDP;
    Network::PortType clusterListenPort = Network::PORT_EPHEMERAL;
    Network::PortType handshakeListenPort = Network::PORT_EPHEMERAL;
  };

  struct IngressSocketOption
  {
    Network::Ingress::IngressTransportType type;
    uint16_t port;
    std::string ExtraArgs;
  };
  TransportOptions transport;
  NodeCapability capabilities = DefaultNodeCapabilities;
  std::vector<IngressSocketOption> ingressSockets;
  // Optional upstream registry; database capability is independent of this
  // address.
  Network::SocketAddress dbHandshakeAddress;
};
} // namespace AtlasNet
