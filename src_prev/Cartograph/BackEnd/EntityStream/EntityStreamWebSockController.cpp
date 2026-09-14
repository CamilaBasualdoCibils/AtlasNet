#include "EntityStreamWebSockController.hpp"
#include "WebBackend.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/entity/EntityLedgerRPC.hpp"

#include "atlasnet/core/node/NodeRegistry.hpp"
#include "enviroment/Enviroment.hpp"
#include <algorithm>
#include <execution>
#include <future>
#include <iostream>
#include <iterator>
#include <numeric>
#include <thread>

void AtlasNet::EntityStreamWebSockController::handleNewMessage(
    const WebSocketConnectionPtr& wsConnPtr, std::string&& message,
    const WebSocketMessageType& conn)
{
  // write your application logic here
  logger->info("Received message from WebSocket client: {}", message);

  // wsConnPtr->send(message);
}
void AtlasNet::EntityStreamWebSockController::handleNewConnection(
    const HttpRequestPtr& req, const WebSocketConnectionPtr& wsConnPtr)
{
  auto state = std::make_shared<ConnectionState>();
  /*
    // ---- Parse query params ----
    auto shardParam = req->getParameter("Shard");
    if (!shardParam.empty())
    {
      state->filter.shardId = static_cast<uint32_t>(std::stoul(shardParam));
    }

    auto entityData = req->getParameter("EntityData");
    // Example: EntityData=Transform,Velocity
    if (!entityData.empty())
    {
      std::stringstream ss(entityData);
      std::string item;
      while (std::getline(ss, item, ','))
      {
        state->filter.components.insert(item);
      }
    } */
  wsConnPtr->setContext(state);

  /*   LOG_INFO << "EntityStreamWebSock connected with filter - Shard: "
             << (state->filter.shardId ? std::to_string(*state->filter.shardId)
                                       : "None")
             << ", Components: "
             << (state->filter.components.empty()
                     ? "None"
                     : std::accumulate(
                           state->filter.components.begin(),
                           state->filter.components.end(), std::string(),
                           [](const std::string& a, const std::string& b)
                           { return a + (a.empty() ? "" : ",") + b; }));
                            */

  logger->info("New WebSocket connection established from {}",
               req->getPeerAddr().toIp().c_str());
  std::unique_lock lock(ConnectionMutex);
  connectionStates[wsConnPtr] = state;

  if (!FetchJobRunning.load())
  {
    logger->info(
        "Starting entity data fetch job as this is the first connection.");
    StartFetchJob();
  }
  // write your application logic here
}
void AtlasNet::EntityStreamWebSockController::handleConnectionClosed(
    const WebSocketConnectionPtr& wsConnPtr)
{
  // write your application logic here
  logger->info("WebSocket connection closed.");
  std::unique_lock lock(ConnectionMutex);
  connectionStates.erase(wsConnPtr);

  if (connectionStates.empty())
  {
    logger->info(
        "No more WebSocket connections. Stopping entity data fetch job.");
    FetchJobShouldShutdown.store(true);
    if (fetchEntityDataThread.joinable())
    {
      fetchEntityDataThread.request_stop();
      fetchEntityDataThread.join();
    }
    FetchJobRunning.store(false);
    FetchJobShouldShutdown.store(false);
  }
}
AtlasNet::EntityStreamWebSockController::EntityStreamWebSockController() {}

void AtlasNet::EntityStreamWebSockController::StartFetchJob()
{
  assert((!fetchEntityDataThread.joinable()) && "Fetch job is already running");

  fetchEntityDataThread = std::jthread(
      [this](std::stop_token st)
      {
        auto& backend = AtlasNet::CartographBackendService::GetInstance();
        auto& rpcSystem = backend.GetRPCSystem();

        while (!st.stop_requested() && !FetchJobShouldShutdown.load())
        {
          boost::container::small_vector<AtlasNetShardID, 64> shardIDs;
          backend.GetNodeRegistry().GetAllShardIDs(
              std::back_inserter(shardIDs));

          logger->info("Fetched {} shard IDs", shardIDs.size());

          // -----------------------------
          // 1. Dispatch all RPC calls FIRST (no waiting yet)
          // -----------------------------
          using ResultType =
              std::unordered_map<AtlasNet::AtlasNetEntityID,
                                 AtlasNet::Entity::Components::EntityInfo>;

          std::vector<std::future<TRPCResult<ResultType>>> futures;
          futures.reserve(shardIDs.size());

          for (const auto& shardID : shardIDs)
          {
            logger->info("Dispatching shard {}", shardID.to_string());
            auto shardAddress =
                backend.GetAddressResolver().ResolveShard(shardID);
            if (shardAddress)
            {
              futures.push_back(
                  rpcSystem.Call_R<EntityLedger_GetAllEntitiesInfoRPC>(
                      shardAddress.value(), {}));
            }
          }

          // -----------------------------
          // 2. Collect results (single-threaded, safe .get())
          // -----------------------------
          std::unordered_map<AtlasNet::AtlasNetEntityID,
                             AtlasNet::Entity::Components::EntityInfo>
              entityInfoCache;

          for (auto& fut : futures)
          {
            if (fut.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready)
            {
              try
              {
                auto result = fut.get();
                if (result.has_value())
                {
                  logger->info("Fetched {} entities from shard",
                               result.value().size());

                  for (auto& [entityId, entityInfo] : result.value())
                  {
                    logger->info(
                        "Entity ID: {}\npos: {}", entityId.to_string(),
                        entityInfo.baseInfo.location.position.to_string());
                    entityInfoCache.emplace(entityId, entityInfo);
                  }
                }
                else
                {
                  logger->error("RPC call returned error: {}",
                                boost::describe::enum_to_string(
                                    result.error(), "<UNKNOWN ERROR>"));
                }
              }
              catch (const std::exception& e)
              {
                logger->error("Shard RPC failed: {}", e.what());
              }
            }
            else
            {
              logger->error("Shard RPC timeout");
            }
          }

          // -----------------------------
          // 3. Broadcast to all connections (snapshot first!)
          // -----------------------------
          std::vector<WebSocketConnectionPtr> conns;
          {
            std::lock_guard lock(ConnectionMutex);
            for (auto& [ws, _] : connectionStates)
              conns.push_back(ws);
          }

          _Json payload;

          for (const auto& [entityId, entityInfo] : entityInfoCache)
          {
            _Json entityJson;
            entityInfo.to_json(entityJson);
            payload[entityId.to_string()] = entityJson;
          }

          const std::string message = payload.dump();
          logger->info("EntityStream Response json\n{}", payload.dump(2));
          for (auto& ws : conns)
          {
            ws->send(message);
          }

          // -----------------------------
          // 4. Reschedule
          // -----------------------------
          if (FetchJobShouldShutdown.load())
          {
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
      });

  FetchJobRunning.store(true);
}