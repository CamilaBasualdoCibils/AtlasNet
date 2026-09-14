#pragma once

#include "boost/describe/enum.hpp"
namespace AtlasNet
{
enum class ServiceAdapterType
{
  DOCKER,
  KUBERNETES,
  INVALID
};
BOOST_DESCRIBE_ENUM(ServiceAdapterType, DOCKER, KUBERNETES, INVALID)
} // namespace AtlasNet