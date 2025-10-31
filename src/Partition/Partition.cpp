#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ServerRegistry.hpp"
#include "Heuristic/Shape.hpp"

Partition::Partition()
{
}
Partition::~Partition()
{
}
void Partition::Init()
{
	InterLinkIdentifier partitionIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
	
	logger = std::make_shared<Log>(partitionIdentifier.ToString());
	Interlink::Get().Init(
		InterlinkProperties{
			.ThisID = partitionIdentifier,
			.logger = logger,
			.callbacks = {.acceptConnectionCallback = [](const Connection &c)
						  { return true; },
						  .OnConnectedCallback = [this](const InterLinkIdentifier &Connection) 
              {
                logger->Debug("[Partition] connected callback");
                if (Connection.Type == InterlinkType::eGameServer)
                {
                  ConnectedGameServer = std::make_unique<InterLinkIdentifier>(Connection);
                  logger->DebugFormatted("[Partition] ready to send messages to GameServer: {}", Connection.ToString().c_str());
                }
              },
						  .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {Partition::Get().MessageArrived(fromWhom,data);}}});

	std::string TestMessageStr = "Hello This is a test message from " + partitionIdentifier.ToString();

	Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(TestMessageStr)));
	// InterlinkMessage message;
	// message.SendTo(InterLinkIdentifier::MakeIDGod());
	// std::shared_ptr<dataPacket = std::make_shared<InterlinkDataPacket>();
	// Interlink::Get().SendMessage();
	while (!ShouldShutdown)
	{

		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}

void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{
    AtlasNetMessageHeader header = AtlasNetMessageHeader::Null;
    std::vector<AtlasEntity> entities;

    // ------------------------------------------------------------------------
    // Check if this message is a valid entity packet 
    // NOTE: should cache as unordered_map or set using entity IDs instead
    // ------------------------------------------------------------------------
    if (ParseEntityPacket(data, header, entities))
    {
        logger->DebugFormatted("[Partition] Received {} entities from {} with header {}",
                               entities.size(), fromWhom.target.ToString(), static_cast<int>(header));
        // Cache entities
        switch (header)
        {
            case AtlasNetMessageHeader::EntityUpdate:
            {
                CachedEntities = entities;
                return;
              }
              case AtlasNetMessageHeader::EntityIncoming:
              {
                for (const auto &entity : entities)
                    CachedEntities.push_back(entity);
                break;
              }              
              case AtlasNetMessageHeader::EntityOutgoing:
              {
                for (const auto &entity : entities)
                {
                    CachedEntities.erase(
                        std::remove_if(CachedEntities.begin(), CachedEntities.end(),
                        [&](const AtlasEntity &e) { return e.ID == entity.ID; }),
                        CachedEntities.end());
                }
                break;
              }
              default:
                logger->WarningFormatted("[Partition] Unknown header {}", static_cast<int>(header));
              break;
            }
            
        // Forward to GameServer
        if (ConnectedGameServer != nullptr)
        {
            std::vector<std::byte> buffer;
            buffer.reserve(1 + entities.size() * sizeof(AtlasEntity));
            buffer.push_back(static_cast<std::byte>(header));
            for (const auto &entity : entities)
            {
                const std::byte *ptr = reinterpret_cast<const std::byte *>(&entity);
                buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));
            }
            Interlink::Get().SendMessageRaw(*ConnectedGameServer, std::span(buffer));
        }

        return;
    }

	std::string msg(reinterpret_cast<const char*>(std::data(data)), std::size(data));
	if(msg == "Fetch Shape")
	{
		// Get our own partition ID
		InterLinkIdentifier partitionId(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
		
		// Initialize database connection
		auto cache = std::make_unique<RedisCacheDatabase>();
		if (!cache->Connect())
		{
			logger->Error("Failed to connect to cache database");
			return;
		}

		// The key in the database uses our partition ID
		std::string key = partitionId.ToString();
		
		// Try to fetch our shape data
		std::optional<std::string> shapeData = cache->HashGet(m_PartitionShapeManifest,key);
		if (!shapeData.has_value())
		{
			logger->ErrorFormatted("No shape found for partition {}", partitionId.ToString());
			return;
		}

		// Parse the shape data and construct our shape
		try 
		{
			Shape shape;
			std::string data = shapeData.value();
			
			// Find the triangles section
			size_t trianglesStart = data.find("triangles:");
			if (trianglesStart == std::string::npos)
			{
				logger->Error("Invalid shape data format - no triangles section");
				return;
			}

			// Parse the triangle vertices
			size_t pos = trianglesStart + 10; // Skip "triangles:"
			while (pos < data.length())
			{
				if (data.substr(pos, 9) == "triangle:")
				{
					Triangle triangle;
					pos += 9; // Skip "triangle:"
					
					// Parse 3 vertices for this triangle
					for (int i = 0; i < 3; i++)
					{
						size_t vStart = data.find("v(", pos);
						if (vStart == std::string::npos) break;
						
						size_t comma = data.find(",", vStart);
						size_t end = data.find(")", vStart);
						if (comma == std::string::npos || end == std::string::npos) break;
						
						float x = std::stof(data.substr(vStart + 2, comma - (vStart + 2)));
						float y = std::stof(data.substr(comma + 1, end - (comma + 1)));
						
						triangle[i] = vec2(x, y);
						pos = end + 1;
					}
					
					shape.triangles.push_back(triangle);
					pos = data.find("triangle:", pos);
					if (pos == std::string::npos) break;
				}
				else break;
			}

			// Assign the shape to our member variable
			partitionShape = std::move(shape);
			logger->DebugFormatted("Successfully loaded shape with {} triangles", partitionShape.triangles.size());
		}
		catch (const std::exception& e)
		{
			logger->ErrorFormatted("Error parsing shape data: {}", e.what());
			return;
		}
	}
}

// ============================================================================
// Helper: ParseEntityPacket
// Validates and extracts AtlasEntities and header type
// ============================================================================
bool Partition::ParseEntityPacket(std::span<const std::byte> data,
                                  AtlasNetMessageHeader &outHeader,
                                  std::vector<AtlasEntity> &outEntities)
{
    if (data.size() < 1)
        return false;

    // Read header
    outHeader = static_cast<AtlasNetMessageHeader>(data[0]);
    const std::byte *payload = data.data() + 1;
    const size_t payloadSize = data.size() - 1;

    // Check alignment
    if (payloadSize == 0 || payloadSize % sizeof(AtlasEntity) != 0)
        return false;

    const size_t entityCount = payloadSize / sizeof(AtlasEntity);
    outEntities.resize(entityCount);
    std::memcpy(outEntities.data(), payload, payloadSize);

    return true;
}
