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
						  .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
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
			
			// Parse metadata first
			size_t shapeIdStart = data.find("shape_id:");
			if (shapeIdStart != std::string::npos) {
				size_t shapeIdEnd = data.find(";", shapeIdStart);
				if (shapeIdEnd != std::string::npos) {
					std::string shapeId = data.substr(shapeIdStart + 9, shapeIdEnd - (shapeIdStart + 9));
					logger->DebugFormatted("Loading shape ID: {}", shapeId);
				}
			}

			// Find the triangles section
			size_t trianglesStart = data.find("triangles:");
			if (trianglesStart == std::string::npos)
			{
				logger->Error("Invalid shape data format - no triangles section");
				return;
			}

			// Parse the triangle vertices
			size_t pos = trianglesStart + 9; // Skip "triangles:"
			while (pos < data.length())
			{
				// Find next triangle marker
				pos = data.find("triangle:", pos);
				if (pos == std::string::npos) break;
				pos += 9; // Skip "triangle:"

				Triangle triangle;
				bool validTriangle = true;
				
				// Parse 3 vertices for this triangle
				for (int i = 0; i < 3; i++)
				{
					size_t vStart = data.find("v(", pos);
					if (vStart == std::string::npos) {
						validTriangle = false;
						break;
					}
					
					size_t comma = data.find(",", vStart);
					size_t end = data.find(")", vStart);
					if (comma == std::string::npos || end == std::string::npos) {
						validTriangle = false;
						break;
					}
					
					try {
						float x = std::stof(data.substr(vStart + 2, comma - (vStart + 2)));
						float y = std::stof(data.substr(comma + 1, end - (comma + 1)));
						triangle[i] = vec2(x, y);
						logger->DebugFormatted("Parsed vertex {}: ({}, {})", i, x, y);
						pos = end + 1;
					} catch (const std::exception& e) {
						logger->ErrorFormatted("Error parsing vertex: {}", e.what());
						validTriangle = false;
						break;
					}
				}
				
				if (validTriangle) {
					shape.triangles.push_back(triangle);
					logger->Debug("Added triangle to shape");
				} else {
					logger->Error("Failed to parse triangle vertices");
				}
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
