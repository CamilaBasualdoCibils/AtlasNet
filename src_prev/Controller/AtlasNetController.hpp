
#pragma once
#include "adapter/IServiceAdapter.hpp"

#include "atlasnet/core/node/AtlasNetNode.hpp"

#include "atlasnet/core/universe/UniverseEvents.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include <shared_mutex>
#
#include <memory>

namespace AtlasNet
{

class AtlasNetController : public IAtlasNetNode
{

public:
  AtlasNetController();
  void OnInit() override;

private:
  void OnShutdown() override;

  void OnWorldCreated(const WorldCreatedEvent& event);

  void LoadStartupWorlds();

  std::unique_ptr<IServiceAdapter> CreateServiceAdapter(
      const std::string_view& serviceName, const std::string_view& imageName,
      std::vector<std::pair<Network::PortType, Network::PortType>> portMappings = {});

  std::shared_mutex _adapterMutex;
  std::unordered_map<WorldID, std::unique_ptr<IServiceAdapter>>
      _serviceAdapters;
  std::unique_ptr<IServiceAdapter> _GatewayServiceAdapter;
};
} // namespace AtlasNet
