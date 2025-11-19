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

#include "AtlasNet/AtlasNetBootstrap.hpp"
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
  Interlink::Get().Init(InterLinkProps);

  SetPartitionCount(10);
  // std::this_thread::sleep_for(std::chrono::seconds(4));
  // god.removePartition(4);
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
  Cleanup();
  Interlink::Get().Shutdown();
}

void God::HeuristicUpdate()
{
  computeAndStorePartitions();
  notifyPartitionsToFetchShapes();
}

void God::handleOutlierMessage(const std::string& message)
{
  // Handle "OutliersDetected:partitionId:count" messages
  if (message.starts_with("OutliersDetected:")) {
    // Find the last colon to get the count (since partitionId may contain spaces)
    size_t lastColon = message.rfind(':');
    if (lastColon == std::string::npos || lastColon == 16) { // 16 = length of "OutliersDetected:"
      logger->Error("Invalid OutliersDetected message format - missing count");
      return;
    }
    
    // Extract partition ID (everything between "OutliersDetected:" and last colon)
    std::string partitionId = message.substr(17, lastColon - 17);
    
    // Validate and parse outlier count
    std::string countStr = message.substr(lastColon + 1);
    if (countStr.empty()) {
      logger->ErrorFormatted("Empty outlier count in OutliersDetected message: {}", message);
      return;
    }
    
    // Check if string contains only digits
    if (countStr.find_first_not_of("0123456789") != std::string::npos) {
      logger->ErrorFormatted("Invalid outlier count format (non-numeric): '{}' in message: {}", countStr, message);
      return;
    }
    
    int outlierCount;
    try {
      outlierCount = std::stoi(countStr);
    } catch (const std::invalid_argument& e) {
      logger->ErrorFormatted("Invalid outlier count format in OutliersDetected message: '{}' - Error: {}", countStr, e.what());
      return;
    } catch (const std::out_of_range& e) {
      logger->ErrorFormatted("Outlier count out of range in OutliersDetected message: '{}' - Error: {}", countStr, e.what());
      return;
    }
    
    logger->DebugFormatted("Received outlier notification from {}: {} outliers", partitionId, outlierCount);
    
    // Process outliers for this partition
    processOutliersForPartition(partitionId);
  }
  // Handle "EntityRemoved:partitionId:entityId:x:y:z" messages from partitions
  else if (message.starts_with("EntityRemoved:")) {
    // Find all colons in the message
    std::vector<size_t> colonPositions;
    size_t pos = 0;
    while ((pos = message.find(':', pos + 1)) != std::string::npos) {
      colonPositions.push_back(pos);
    }
    
    logger->DebugFormatted("DEBUG: EntityRemoved message: '{}'", message);
    logger->DebugFormatted("DEBUG: Found {} colons at positions: ", colonPositions.size());
    for (size_t i = 0; i < colonPositions.size(); ++i) {
      logger->DebugFormatted("DEBUG: Colon {} at position {}", i, colonPositions[i]);
    }
    
    if (colonPositions.size() < 5) {
      logger->ErrorFormatted("Invalid EntityRemoved message format - not enough colons (found {}, need 5)", colonPositions.size());
      return;
    }
    
    // Extract components from the end
    size_t zColon = colonPositions[colonPositions.size() - 1];
    size_t yColon = colonPositions[colonPositions.size() - 2];
    size_t xColon = colonPositions[colonPositions.size() - 3];
    size_t entityIdColon = colonPositions[colonPositions.size() - 4];
    size_t partitionIdColon = colonPositions[colonPositions.size() - 5];
    
    std::string sourcePartition = message.substr(14, partitionIdColon - 14);
    
    // Validate and parse entity ID
    std::string entityIdStr = message.substr(entityIdColon + 1, xColon - (entityIdColon + 1));
    if (entityIdStr.empty()) {
      logger->ErrorFormatted("Empty entity ID in EntityRemoved message: {}", message);
      return;
    }
    
    // Check if string contains only digits
    if (entityIdStr.find_first_not_of("0123456789") != std::string::npos) {
      logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, message);
      return;
    }
    
    AtlasEntityID entityId;
    try {
      entityId = std::stoi(entityIdStr);
    } catch (const std::invalid_argument& e) {
      logger->ErrorFormatted("Invalid entity ID format in EntityRemoved message: '{}' - Error: {}", entityIdStr, e.what());
      return;
    } catch (const std::out_of_range& e) {
      logger->ErrorFormatted("Entity ID out of range in EntityRemoved message: '{}' - Error: {}", entityIdStr, e.what());
      return;
    }
    
    // Validate and parse coordinates
    std::string xStr = message.substr(xColon + 1, yColon - (xColon + 1));
    std::string yStr = message.substr(yColon + 1, zColon - (yColon + 1));
    std::string zStr = message.substr(zColon + 1);
    
    // Helper function to validate and parse float coordinates
    auto parseFloatCoordinate = [&](const std::string& coordStr, const std::string& coordName) -> std::optional<float> {
      if (coordStr.empty()) {
        logger->ErrorFormatted("Empty {} coordinate in EntityRemoved message: {}", coordName, message);
        return std::nullopt;
      }
      
      // Check if string contains valid float characters (digits, decimal point, minus sign)
      if (coordStr.find_first_not_of("0123456789.-") != std::string::npos) {
        logger->ErrorFormatted("Invalid {} coordinate format: '{}' in message: {}", coordName, coordStr, message);
        return std::nullopt;
      }
      
      try {
        return std::stof(coordStr);
      } catch (const std::invalid_argument& e) {
        logger->ErrorFormatted("Invalid {} coordinate format in EntityRemoved message: '{}' - Error: {}", coordName, coordStr, e.what());
        return std::nullopt;
      } catch (const std::out_of_range& e) {
        logger->ErrorFormatted("{} coordinate out of range in EntityRemoved message: '{}' - Error: {}", coordName, coordStr, e.what());
        return std::nullopt;
      }
    };
    
    auto xOpt = parseFloatCoordinate(xStr, "X");
    auto yOpt = parseFloatCoordinate(yStr, "Y");
    auto zOpt = parseFloatCoordinate(zStr, "Z");
    
    if (!xOpt || !yOpt || !zOpt) {
      logger->ErrorFormatted("Failed to parse coordinates in EntityRemoved message: {}", message);
      return;
    }
    
    float x = *xOpt;
    float y = *yOpt;
    float z = *zOpt;
    
    logger->DebugFormatted("Received entity removal confirmation: Entity {} from {} at ({}, {}, {})", 
      entityId, sourcePartition, x, y, z);
    
    // Create entity from the received data
    AtlasEntity entity;
    entity.ID = entityId;
    entity.Position = vec3{x, y, z};
    
    // Find the correct target partition for this entity
    redistributeEntityToCorrectPartition(entity, sourcePartition);
  }
}


std::vector<std::string> God::GetPartitionIDs()
{
  std::string serviceResp = DockerIO::Get().request("GET", "/services/"+AtlasNetBootstrap::Get().PartitionServiceName);
  Json serviceJson = Json::parse(serviceResp);
  std::string serviceID = serviceJson["ID"];

  std::string tasksResp = DockerIO::Get().request("GET", "/tasks?filters={\"service\":{\"" + serviceID + "\":true}}");
  Json tasksJson = Json::parse(tasksResp);
  std::vector<std::string> instanceNames;
  for (auto &task : tasksJson)
  {
    if (task.contains("Status") && task["Status"].contains("ContainerStatus"))
    {
      auto &cstatus = task["Status"]["ContainerStatus"];
      if (cstatus.contains("ContainerID"))
      {
        std::string containerID = cstatus["ContainerID"];
        // Optional: inspect the container to get its name
        std::string contResp = DockerIO::Get().request("GET", "/containers/" + containerID + "/json");
        Json contJson = Json::parse(contResp);
        if (contJson.contains("Name"))
          instanceNames.push_back(contJson["Name"].get<std::string>());
      }
    }
  }
  for (auto &name : instanceNames)
    std::cout << "Instance: " << name << std::endl;
  return instanceNames;
}

void God::Cleanup()
{
}

void God::SetPartitionCount(uint32 NewCount)
{
  std::string inspectResp = DockerIO::Get().request("GET", "/services/"+AtlasNetBootstrap::Get().PartitionServiceName);
  auto inspectJson = Json::parse(inspectResp);

  try
  {
    int version = inspectJson["Version"]["Index"];
    auto spec = inspectJson["Spec"];
    spec["Mode"]["Replicated"]["Replicas"] = NewCount;
    // 2. Send the update request with the new replica count
    std::string updatePath = "/services/"+AtlasNetBootstrap::Get().PartitionServiceName+"/update?version=" + std::to_string(version);
    std::string updateResp = DockerIO::Get().request("POST", updatePath, &spec);
    if (!updateResp.empty())
    {
      logger->DebugFormatted("Service update responded with \n{}",Json::parse(updateResp).dump(4));
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    logger->ErrorFormatted("Response from service inspect \n{}", inspectJson.dump(4));
    throw "SHIT";
  }

  logger->DebugFormatted("Scaled {} to {} replicas",AtlasNetBootstrap::Get().PartitionServiceName, NewCount);
  PartitionCount = NewCount;
}