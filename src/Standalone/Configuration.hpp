#pragma once
#include "AtlasNet/Node/AtlasNetNode.hpp"
namespace AtlasNet::Standalone
{
struct Configuration
{
  NodeConfig node;
  std::string valkeyURI;
  bool help = false;
};
uint16_t ParsePort(const std::string& value);
NodeCapability ParseCapabilities(const std::string& value);
Configuration ParseConfiguration(int argc, const char* const* argv);
} // namespace AtlasNet::Standalone
