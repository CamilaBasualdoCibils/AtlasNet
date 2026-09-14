#pragma once

#include <boost/describe.hpp>
#include <cstdint>
namespace AtlasNet::Network::Cluster
{

enum class ClusterTransportType : uint8_t
{
  INVALID = 0,
  UDP = 1,
  DPDK = 2,
};
BOOST_DESCRIBE_ENUM(ClusterTransportType, UDP, DPDK, INVALID)

} // namespace AtlasNet::Network::Cluster