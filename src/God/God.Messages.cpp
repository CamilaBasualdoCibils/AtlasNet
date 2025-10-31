#include "God.hpp"
#include "Database/EntityManifest.hpp"
#include "Database/ServerRegistry.hpp"
#include "Interlink/InterlinkEnums.hpp"

void God::handleOutliersDetectedMessage(const std::string& message)
{
  size_t lastColon = message.rfind(':');
  if (lastColon == std::string::npos || lastColon == 16) {
    logger->Error("Invalid OutliersDetected message format - missing count");
    return;
  }

  std::string partitionId = message.substr(17, lastColon - 17);
  std::string countStr = message.substr(lastColon + 1);
  if (countStr.empty()) {
    logger->ErrorFormatted("Empty outlier count in OutliersDetected message: {}", message);
    return;
  }
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
  processOutliersForPartition(partitionId);
}

void God::handleEntityRemovedMessage(const std::string& message)
{
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

  size_t zColon = colonPositions[colonPositions.size() - 1];
  size_t yColon = colonPositions[colonPositions.size() - 2];
  size_t xColon = colonPositions[colonPositions.size() - 3];
  size_t entityIdColon = colonPositions[colonPositions.size() - 4];
  size_t partitionIdColon = colonPositions[colonPositions.size() - 5];

  std::string sourcePartition = message.substr(14, partitionIdColon - 14);

  std::string entityIdStr = message.substr(entityIdColon + 1, xColon - (entityIdColon + 1));
  if (entityIdStr.empty()) {
    logger->ErrorFormatted("Empty entity ID in EntityRemoved message: {}", message);
    return;
  }
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

  auto parseFloatCoordinate = [&](const std::string& coordStr, const std::string& coordName) -> std::optional<float> {
    if (coordStr.empty()) {
      logger->ErrorFormatted("Empty {} coordinate in EntityRemoved message: {}", coordName, message);
      return std::nullopt;
    }
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

  std::string xStr = message.substr(xColon + 1, yColon - (xColon + 1));
  std::string yStr = message.substr(yColon + 1, zColon - (yColon + 1));
  std::string zStr = message.substr(zColon + 1);

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

  AtlasEntity entity;
  entity.ID = entityId;
  entity.Position = vec3{x, y, z};

  redistributeEntityToCorrectPartition(entity, sourcePartition);
}

void God::handleOutlierMessage(const std::string& message)
{
  if (message.starts_with("OutliersDetected:")) {
    handleOutliersDetectedMessage(message);
  } else if (message.starts_with("EntityRemoved:")) {
    handleEntityRemovedMessage(message);
  }
}


