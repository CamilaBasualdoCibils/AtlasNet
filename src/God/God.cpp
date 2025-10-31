#include "God.hpp"
#include "Docker/DockerIO.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ShapeManifest.hpp"
#include "Database/GridCellManifest.hpp"
#include "Database/EntityManifest.hpp"
#include "Database/PartitionEntityManifest.hpp"
#include "Utils/GeometryUtils.hpp"
#include <ctime>
#include "Database/ServerRegistry.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "Interlink/Connection.hpp"
God::God()
{
}

God::~God()
{
}

void God::ClearAllDatabaseState()
{
  // Ensure cache is initialized
  if (!cache)
  {
    cache = std::make_unique<RedisCacheDatabase>();
    cache->Connect();
  }
  logger->Debug("Clearing all entity and shape data from database and memory cache");
  ShapeManifest::Clear(cache.get());
  GridCellManifest::Clear(cache.get());  // Clear grid cell data too
  EntityManifest::ClearAllOutliers(cache.get());
  
  // Clear ALL partition entity manifests to prevent stale data from previous runs
  PartitionEntityManifest::ClearAllPartitionManifests(cache.get());
  
  // Clear memory cache
  shapeCache.shapes.clear();
  shapeCache.partitionToShapeIndex.clear();
  shapeCache.isValid = false;
}

void God::Init()
{
  logger->Debug("Init");
  ClearAllDatabaseState();
  InterlinkProperties InterLinkProps;
  InterLinkProps.callbacks = InterlinkCallbacks{
      .acceptConnectionCallback = [](const Connection &c)
      { return true; },
      .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
      .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {
        std::string msg(reinterpret_cast<const char*>(std::data(data)), std::size(data));
        God::Get().handleOutlierMessage(msg);
      },
  };
  InterLinkProps.logger = logger;
  InterLinkProps.ThisID = InterLinkIdentifier::MakeIDGod();
  logger->ErrorFormatted("[{}]", InterLinkProps.ThisID.ToString());
  Interlink::Get().Init(InterLinkProps);

  for (int32 i = 1; i <= 12; i++)
  {

    spawnPartition();
  }

  std::cerr << "Servers in the ServerRegistry:\n";
  for (const auto &server : ServerRegistry::Get().GetServers())
  {
    std::cerr << server.second.identifier.ToString() << " " << server.second.address.ToString() << std::endl;
  }

  using clock = std::chrono::steady_clock;
  auto startTime = clock::now();
  auto lastCall = startTime;
  bool firstCalled = false;
  while (!ShouldShutdown)
  {
    auto now = clock::now();
    Interlink::Get().Tick();
    // First call after 10 seconds to compute initial partition shapes
    if (!firstCalled && now - startTime >= std::chrono::seconds(10))
    {
      firstCalled = true;
      HeuristicUpdate();
      // Process any existing outliers in the database after shapes are computed
      processExistingOutliers();
    }
    // wait for partion messages for redistribution
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  logger->Debug("Shutting down");
  cleanupContainers();
  Interlink::Get().Shutdown();
}

void God::HeuristicUpdate()
{
  computeAndStorePartitions();
  notifyPartitionsToFetchShapes();
}

const decltype(God::ActiveContainers) &God::GetContainers()
{
  return ActiveContainers;
}

const God::ActiveContainer &God::GetContainer(const DockerContainerID &id)
{

  const ActiveContainer &v = *ActiveContainers.get<IndexByID>().find(id);
  return v;
}

std::optional<God::ActiveContainer> God:: spawnPartition()
{
  Json createRequestBody =
      {
          {"Image", "partition"},
          // {"Cmd", {"--testABCD"}},
          {"Env", Json::array({"ATLAS_LOG_LEVEL=WARNING"})},
          {"ExposedPorts", {}},
          {"HostConfig", {{"PublishAllPorts", true}, {"NetworkMode", "AtlasNet"}, // <-- attach to your custom network
                          {"Binds", Json::array({"/var/run/docker.sock:/var/run/docker.sock"})},
                          {"CapAdd", Json::array({"SYS_PTRACE"})},
                          {"SecurityOpt", Json::array({"seccomp=unconfined"})}}},
          {"NetworkingConfig", {{"EndpointsConfig", {
                                                        {"AtlasNet", Json::object()} // tell Docker to connect to that network
                                                    }}}}};

  std::string createResp = DockerIO::Get().request("POST", "/containers/create", &createRequestBody);

  auto createRespJ = nlohmann::json::parse(createResp);

  ActiveContainer newPartition;
  // logger->Debug(createRespJ.dump(4));
  newPartition.ID = createRespJ["Id"].get<std::string>();
  newPartition.LatestInformJson = nlohmann::json::parse(DockerIO::Get().InspectContainer(newPartition.ID));
  std::string StartResponse = DockerIO::Get().request("POST", std::string("/containers/").append(newPartition.ID).append("/start"));
  logger->DebugFormatted("Created container with ID {}", newPartition.ID); //, newPartition.LatestInformJson.dump(4));
  ActiveContainers.insert(newPartition);

  return newPartition;
}

bool God::removePartition(const DockerContainerID &id, uint32 TimeOutSeconds)
{
  logger->DebugFormatted("Stopping {}", id);
  std::string RemoveResponse = DockerIO::Get().request("POST", "/containers/" + std::string(id) + "/stop?t=" + std::to_string(TimeOutSeconds));
  logger->DebugFormatted("Deleting {}", id);
  std::string DeleteResponse = DockerIO::Get().request("DELETE", "/containers/" + id);

  // Clear partition entity manifest for removed partition
  // Note: We can't get the exact partition ID from container ID, but we can clear all manifests
  // This is a safety measure to prevent stale data
  logger->Debug("CLEARED: Partition removed - stale entity data may remain in database");

  return true;
}

bool God::cleanupContainers()
{
  for (const auto &Partition : ActiveContainers)
  {
    removePartition(Partition.ID);
  }
  ActiveContainers.clear();
  return true;
}