
#include "WebBackend.hpp"

#include <drogon/drogon.h>




int main(int argc, char** argv)
{

  AtlasNet::CartographBackendService::GetInstance().Init();
  return 0;
}
void AtlasNet::CartographBackendService::OnInit()
{
  GetLogger()->info("Initializing AtlasNet Web Backend...");

  // Initialize and start the Drogon HTTP server
  drogon::app().addListener("0.0.0.0", 2000);
  Setup();
  drogon::app().run();
}
void AtlasNet::CartographBackendService::OnShutdown()
{
  GetLogger()->info("Shutting down AtlasNet Web Backend...");
  // Perform any necessary cleanup before shutdown
  drogon::app().quit();
}
void AtlasNet::CartographBackendService::Setup()
{
  entityStreamWebSock.emplace();
  // drogon::app().registerHandler(
  //     "/api/entity-fetch",
  //     [this](const drogon::HttpRequestPtr& req,
  //            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
  //     { HandleEntityFetchRequest(req, std::move(callback)); });
}