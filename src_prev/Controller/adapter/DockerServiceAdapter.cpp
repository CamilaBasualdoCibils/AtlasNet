#include "DockerServiceAdapter.hpp"
#include "enviroment/Enviroment.hpp"
#include <thread>

void AtlasNet::DockerServiceAdapter::Create()
{
  if (Exists())
  {
    return;
  }

  _Json ports = _Json::array();

  for (const auto& [publishedPort, targetPort] : _portMappings)
  {
    ports.push_back({
        {"Protocol", "udp"}, // change if you support tcp
        {"PublishedPort", publishedPort},
        {"TargetPort", targetPort},
    });
  }

  _Json body = {
      {"Name", _serviceName},
      {"TaskTemplate",
       {
           {"ContainerSpec",
            {
                {"Image", _imageName},
                {"CapabilityAdd", {"SYS_PTRACE"}},
                {"TTY", true}, //ensures color terminal
                /*  {"Env",
                  {
                      "NODE_IP={{.Node.Hostname}}",
                  }}, */
            }},
           {"RestartPolicy",
            {
                {"Condition", "any"},
            }},
       }},
      {"Mode",
       {
           {"Replicated",
            {
                {"Replicas", 0}, // start at 0 replicas
            }},
       }},
      {"Networks", _Json::array({{{"Target", Env::Docker_NetworkName}}})},

      {"EndpointSpec",
       {
           {"Ports", ports},
       }},
  };

  auto response = Request("POST", "/v1.43/services/create", body.dump(),
                          "application/json");

  if (response.status < 200 || response.status >= 300)
  {
    throw std::runtime_error("Failed to create docker service: " +
                             response.body.dump());
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
}
bool AtlasNet::DockerServiceAdapter::Exists() const
{
  auto response = Request("GET", "/v1.43/services/" + _serviceName);

  if (response.status == 404)
  {
    return false;
  }

  if (response.status >= 200 && response.status < 300)
  {
    return true;
  }

  throw std::runtime_error("Failed to query docker service: " +
                           response.body.dump());
}
void AtlasNet::DockerServiceAdapter::Destroy()
{
  auto response = Request("DELETE", "/v1.43/services/" + _serviceName);

  if (response.status == 404)
  {
    return;
  }

  if (response.status < 200 || response.status >= 300)
  {
    throw std::runtime_error("Failed to delete docker service: " +
                             response.body.dump());
  }
}
void AtlasNet::DockerServiceAdapter::SetReplicaCount(uint32_t count)
{
  auto service = Request("GET", "/v1.43/services/" + _serviceName);

  if (service.status != 200)
  {
    throw std::runtime_error("Failed to fetch service: " + service.body.dump());
  }

  const auto version = service.body["Version"]["Index"].get<uint64_t>();

  service.body["Spec"]["Mode"]["Replicated"]["Replicas"] = count;

  auto update = Request("POST",
                        "/v1.43/services/" + _serviceName +
                            "/update?version=" + std::to_string(version),
                        service.body["Spec"].dump(), "application/json");

  if (update.status < 200 || update.status >= 300)
  {
    throw std::runtime_error("Failed to update replicas: " +
                             update.body.dump());
  }
}
uint32_t AtlasNet::DockerServiceAdapter::GetReplicaCount() const
{
  auto response = Request("GET", "/v1.43/services/" + _serviceName);

  if (response.status != 200)
  {
    throw std::runtime_error("Failed to get service: " + response.body.dump());
  }

  try
  {
    return response.body["Spec"]["Mode"]["Replicated"]["Replicas"]
        .get<uint32_t>();
  }
  catch (...)
  {
    return 0;
  }
}
