
#include "AtlasNetController.hpp"
#include "adapter/DockerServiceAdapter.hpp"
#include "adapter/IServiceAdapter.hpp"
#include "adapter/KubernetesServiceAdapter.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include "atlasnet/core/universe/WorldEnums.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "enviroment/Enviroment.hpp"
#include <format>
#include <utility>

void AtlasNet::AtlasNetController::OnInit()
{
  GetLogger()->info("Initializing AtlasNet Controller...");


  if (Env::ControllerServiceBackend == ServiceAdapterType::INVALID)
  {
    GetLogger()->error(
        "Invalid service adapter type specified in "
        "ATLASNET_CONTROLLER_SERVICE_BACKEND. No service adapters will be "
        "created.");

        GetLogger()->error(
        "Received value: {}",
        std::getenv("ATLASNET_CONTROLLER_SERVICE_BACKEND"));
    throw std::runtime_error(
        "Invalid service adapter type specified in environment variable "
        "ATLASNET_CONTROLLER_SERVICE_BACKEND");
  }

  GetGlobalEventSystem().On<WorldCreatedEvent>(
      [this](const WorldCreatedEvent& event) { OnWorldCreated(event); });

  GetLogger()->info("Creating gateway service adapter...");
  _GatewayServiceAdapter =
      CreateServiceAdapter("atlasnet-gateway", "atlasnet-gateway-dev",
                           {{Env::GatewayListenPort, Env::GatewayListenPort}});
  _GatewayServiceAdapter->Create();
  GetLogger()->info("Scaling gateway service adapter to 1 replica...");
  _GatewayServiceAdapter->SetReplicaCount(1);
  LoadStartupWorlds();
}
void AtlasNet::AtlasNetController::OnShutdown()
{
  std::unique_lock lock(_adapterMutex);
  GetLogger()->info("Shutting down AtlasNet Controller...");
  for (auto& [worldID, adapter] : _serviceAdapters)
  {
    GetLogger()->info("Destroying service adapter for world ID {}", worldID.to_string());
    adapter->Destroy();
    GetLogger()->info("Destroyed service adapter for world ID {}", worldID.to_string());
  }
  _serviceAdapters.clear();
  if (_GatewayServiceAdapter)
  {
    GetLogger()->info("Destroying gateway service adapter...");
    _GatewayServiceAdapter->Destroy();
    GetLogger()->info("Destroyed gateway service adapter.");
  }
}
AtlasNet::AtlasNetController::AtlasNetController()
    : IAtlasNetNode(AtlasNetNodeType::Controller)
{
}
void AtlasNet::AtlasNetController::OnWorldCreated(
    const WorldCreatedEvent& event)
{
  IServiceAdapter* adapter = nullptr;
  {
    std::unique_lock lock(_adapterMutex);
    GetLogger()->info("Received WorldCreatedEvent for world {} with ID {}", event.worldName, event.worldID.to_string());
    _serviceAdapters.emplace(
        event.worldID,
        CreateServiceAdapter("atlasnet-shard-" + event.worldID.to_string(),
                             Env::ShardDefaultImage));
    adapter = _serviceAdapters[event.worldID].get();
  }
  GetLogger()->info("Created service adapter for world {} with ID {}", event.worldName, event.worldID.to_string());
  adapter->Create();
  GetLogger()->info("Called Create on service adapter for world {} with ID {}", event.worldName, event.worldID.to_string());
  adapter->SetReplicaCount(1);
  GetLogger()->info("Set replica count to 1 on service adapter for world {} with ID {}", event.worldName, event.worldID.to_string());
}

void AtlasNet::AtlasNetController::LoadStartupWorlds()
{
  /*
 example of expected format for Env::StartupWorlds:
{
 "worlds": {
   "MainWorld": {
     "SpaceType": "Cartesian2D",
     "Heuristic": "KDTree"
   },
   "AnotherWorld": {
     "SpaceType": "Cartesian3D",
     "ShardImageOverride": "custom-shard-image:latest"
   }
 }
}
 */
  try
  {

    _JsonOrdered startupWorlds = _JsonOrdered::parse(Env::StartupWorlds);

    for (auto& [worldName, worldConfig] : startupWorlds["worlds"].items())
    {
      GetLogger()->info("Loading startup world: {}", worldName);
      WorldDefinition worldDef;
      worldDef.name = worldName;
      if (worldConfig.contains("SpaceType"))
      {
        std::string spaceTypeStr = worldConfig["SpaceType"].get<std::string>();
        if (spaceTypeStr == "Cartesian2D")
          worldDef.spaceType = WorldSpaceType::Cartesian2D;
        else if (spaceTypeStr == "Cartesian3D")
          worldDef.spaceType = WorldSpaceType::Cartesian3D;
        else
          GetLogger()->warn("Unknown SpaceType '{}' for world {}. Defaulting to Cartesian2D.", spaceTypeStr, worldName);
      }
      else
      {
        GetLogger()->warn("No SpaceType specified for world {}. Defaulting to Cartesian2D.", worldName);
        worldDef.spaceType = WorldSpaceType::Cartesian2D;
      }
      auto [result, worldID] = GetUniverse().CreateWorld(worldDef);
      GetLogger()->info("CreateWorld result: {}\n - worldID: {}\n - worldName: {}\n, - SpaceType: {}",
                        boost::describe::enum_to_string(result, "UNKNOWN RESULT"),
                        worldID->to_string(), worldName,
                        boost::describe::enum_to_string(worldDef.spaceType, "UNKNOWN SPACE TYPE"));
                       
    }
  }
  catch (const std::exception& ex)
  {
    GetLogger()->error("Failed to load startup worlds: {}", ex.what());
    GetLogger()->error("Startup worlds config: {}", Env::StartupWorlds);
  }
}

std::unique_ptr<AtlasNet::IServiceAdapter>
AtlasNet::AtlasNetController::CreateServiceAdapter(
    const std::string_view& serviceName, const std::string_view& imageName,
    std::vector<std::pair<Network::PortType, Network::PortType>> portMappings)
{
  std::string ServiceName = std::string(serviceName);
  // try to detect if this is running in kubernetes
  GetLogger()->info("Creating service adapter for service '{}' with image '{}' using {} backend.",
                    ServiceName, imageName,
                    boost::describe::enum_to_string(Env::ControllerServiceBackend, "UNKNOWN"));
  switch (Env::ControllerServiceBackend)
  {
  case ServiceAdapterType::KUBERNETES:
  {
    return std::make_unique<KubernetesServiceAdapter>(
        ServiceName, imageName, portMappings,
        KubernetesServiceAdapter::ImagePullPolicy::Never);
  }
  break;
  case ServiceAdapterType::DOCKER:
  {
    return std::make_unique<DockerServiceAdapter>(
        Env::DockerSocketPath, ServiceName, imageName, portMappings);
  }
  break;
  case ServiceAdapterType::INVALID:
    throw std::runtime_error(
        "INVALID service adapter type specified in environment variable "
        "ATLASNET_CONTROLLER_SERVICE_BACKEND");
    break;
  };
  /*  if (std::getenv("KUBERNETES_SERVICE_HOST") ||
       Env::ControllerServiceBackend == ServiceAdapterType::Kubernetes)
   {
     GetLogger()->info("Detected Kubernetes environment, using KubernetesServiceAdapter for {}", ServiceName);
     return std::make_unique<KubernetesServiceAdapter>(
         ServiceName, imageName, std::vector<std::pair<PortType, PortType>>{},
         KubernetesServiceAdapter::ImagePullPolicy::Never);
   }
   else
   {
     GetLogger()->info("No Kubernetes environment detected, using default IServiceAdapter for {}", ServiceName);
     throw std::runtime_error("No suitable service adapter found for " +
                              ServiceName);
   } */
}
