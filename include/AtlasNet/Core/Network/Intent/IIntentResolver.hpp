#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Intent/IntentRecepient.hpp"
namespace AtlasNet::Network::Intent
{
class IIntentResolver
{
public:
  virtual ~IIntentResolver() = default;

  virtual std::optional<AtlasNetNodeID>
  ResolveIntent(const VIntent& intent) = 0;
};
} // namespace AtlasNet::Network::Intent