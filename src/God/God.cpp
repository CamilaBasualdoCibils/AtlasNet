#include "God.hpp"
#include "Docker/DockerIO.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include <ctime>
#include "Database/ServerRegistry.hpp"
God::God()
{
}

God::~God()
{
}

void God::Init()
{
  logger->Print("Init");
  InterlinkProperties InterLinkProps;
  InterLinkProps.callbacks = {.acceptConnectionCallback = [](const Connection &c)
                              { return true; },
                              .OnConnectedCallback = [](const InterLinkIdentifier& Connection) {}};
  InterLinkProps.logger = logger;
  InterLinkProps.ThisID = InterLinkIdentifier::MakeIDGod();
  IPAddress ipAddress;
  std::cerr << "Value of _PORT_GOD " << _PORT_GOD << std::endl;
  ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":"+std::to_string(_PORT_GOD));
  std::cerr << "Fetching exposed Ports" << std::endl;
  ServerRegistry::Get().RegisterSelf(InterLinkProps.ThisID,ipAddress);

  Interlink::Get().Init(InterLinkProps);
  for (int32 i = 1; i <= 12; i++)
  {

    spawnPartition();
  }

  std::cerr << "Servers in the ServerRegistry:\n";
  for (const auto& server : ServerRegistry::Get().GetServers())
  {
    std::cerr << server.second.identifier.ToString() << " " << server.second.address.ToString() << std::endl;
  }
  computeAndStorePartitions();
  // std::this_thread::sleep_for(std::chrono::seconds(4));
  // god.removePartition(4);

  while (!ShouldShutdown)
  {
    Interlink::Get().Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  logger->Print("Shutting down");
  cleanupContainers();
  ServerRegistry::Get().DeRegisterSelf(InterLinkProps.ThisID);
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

std::optional<God::ActiveContainer> God::spawnPartition()
{
  Json createRequestBody =
      {
          {"Image", "partition"},
         // {"Cmd", {"--testABCD"}},
          {"ExposedPorts", {}},
          {"HostConfig", {{"PublishAllPorts", true}, {"NetworkMode", "AtlasNet"}, // <-- attach to your custom network
                          {"Binds", Json::array({"/var/run/docker.sock:/var/run/docker.sock"})}}},
          {"NetworkingConfig", {{"EndpointsConfig", {
                                                        {"AtlasNet", Json::object()} // tell Docker to connect to that network
                                                    }}}}};

  std::string createResp = DockerIO::Get().request("POST", "/containers/create", &createRequestBody);

  auto createRespJ = nlohmann::json::parse(createResp);

  ActiveContainer newPartition;
  // logger->Print(createRespJ.dump(4));
  newPartition.ID = createRespJ["Id"].get<std::string>();
  newPartition.LatestInformJson = nlohmann::json::parse(DockerIO::Get().InspectContainer(newPartition.ID));
  std::string StartResponse = DockerIO::Get().request("POST", std::string("/containers/").append(newPartition.ID).append("/start"));
  logger->PrintFormatted("Created container with ID {}", newPartition.ID); //, newPartition.LatestInformJson.dump(4));
  ActiveContainers.insert(newPartition);

  return newPartition;
}

bool God::removePartition(const DockerContainerID &id, uint32 TimeOutSeconds)
{
  logger->PrintFormatted("Stopping {}", id);
  std::string RemoveResponse = DockerIO::Get().request("POST", "/containers/" + std::string(id) + "/stop?t=" + std::to_string(TimeOutSeconds));
  logger->PrintFormatted("Deleting {}", id);
  std::string DeleteResponse = DockerIO::Get().request("DELETE", "/containers/" + id);

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

bool God::computeAndStorePartitions()
{
  try
  {
    // Compute partition shapes using heuristic algorithms
    std::vector<Shape> partitionShapes = heuristic.computePartition();

    logger->PrintFormatted("Computed {} partition shapes", partitionShapes.size());

    // Initialize cache database if not already done
    if (!cache)
    {
      cache = std::make_unique<RedisCacheDatabase>();
      if (!cache->Connect())
      {
        logger->Print("Failed to connect to cache database");
        return false;
      }
    }

    // Serialize and store each shape in the database
    for (size_t i = 0; i < partitionShapes.size(); ++i)
    {
      std::string shapeData;
      shapeData += "shape_id:" + std::to_string(i) + ";";
      shapeData += "triangles:";

      // Serialize triangles as simple string format
      for (const auto &triangle : partitionShapes[i].triangles)
      {
        shapeData += "triangle:";
        for (const auto &vertex : triangle)
        {
          shapeData += "v(" + std::to_string(vertex.x) + "," + std::to_string(vertex.y) + ");";
        }
      }

      // Store in database with key "partition_shape_<index>"
      std::string key = "partition_shape_" + std::to_string(i);

      if (!cache->Set(key, shapeData))
      {
        logger->PrintFormatted("Failed to store shape {} in database", i);
        return false;
      }

      logger->PrintFormatted("Stored shape {} with {} triangles", i, partitionShapes[i].triangles.size());
    }

    // Store metadata about the partition computation
    std::string metadata = "total_shapes:" + std::to_string(partitionShapes.size()) + ";timestamp:" + std::to_string(std::time(nullptr));

    if (!cache->Set("partition_metadata", metadata))
    {
      logger->Print("Failed to store partition metadata");
      return false;
    }

    logger->Print("Successfully computed and stored all partition shapes");
    return true;
  }
  catch (const std::exception &e)
  {
    logger->PrintFormatted("Error in computeAndStorePartitions: {}", e.what());
    return false;
  }
}
