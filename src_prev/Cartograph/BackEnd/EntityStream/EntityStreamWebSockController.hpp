// EntityStreamWebSock.h
#pragma once

#include "atlasnet/core/tasks/TaskSystem.hpp"
#include "spdlog/sinks/stdout_color_sinks-inl.h"

#include <atomic>
#include <drogon/WebSocketController.h>
#include <shared_mutex>
#include <thread>
using namespace drogon;
namespace AtlasNet
{
class EntityStreamWebSockController
    : public drogon::WebSocketController<EntityStreamWebSockController>
{
  struct EntityStreamFilter
  {
    std::optional<uint32_t> shardId;

    // e.g. ["Transform"]
    std::unordered_set<std::string> components;
  };
  struct ConnectionState
  {
    EntityStreamFilter filter;
  };

public:
  /* EntityStreamWebSockController(AtlasNet::JobSystem* jobSystem,
                      AtlasNet::ServiceRegistry* serviceRegistry)
      : jobSystem(jobSystem), serviceRegistry(serviceRegistry)
  {
  } */
  EntityStreamWebSockController();
  void handleNewMessage(const WebSocketConnectionPtr&, std::string&&,
                        const WebSocketMessageType&) override;
  void handleNewConnection(const HttpRequestPtr&,
                           const WebSocketConnectionPtr&) override;
  void handleConnectionClosed(const WebSocketConnectionPtr&) override;
  WS_PATH_LIST_BEGIN
  WS_PATH_ADD("/api/entity-stream");
  // list path definitions here;
  WS_PATH_LIST_END
  private:
  std::shared_mutex ConnectionMutex;
  std::unordered_map<drogon::WebSocketConnectionPtr,
                     std::shared_ptr<ConnectionState>>
      connectionStates;
      std::jthread fetchEntityDataThread;
  void StartFetchJob();
  std::atomic_bool FetchJobRunning{false};
  std::atomic_bool FetchJobShouldShutdown{false};

  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("EntityStreamWebSock");

};
}