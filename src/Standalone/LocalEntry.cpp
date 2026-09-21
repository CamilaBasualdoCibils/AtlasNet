#include "AtlasNet/Node/AtlasNetNode.hpp"
#include "Backends/ValkeyRemoteBackend.hpp"
#include <csignal>
#include <iostream>
#include <sw/redis++/redis++.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace
{
volatile std::sig_atomic_t stopRequested = 0;

void Stop(int)
{
  stopRequested = 1;
}

bool IsValkeyReady(const std::string& uri)
{
  try
  {
    return sw::redis::Redis(uri).ping() == "PONG";
  }
  catch (const std::exception&)
  {
    return false;
  }
}

class LocalValkey
{
public:
  LocalValkey(const std::string& uri, uint16_t port)
  {
    if (IsValkeyReady(uri))
    {
      std::cout << "Using existing Valkey at " << uri << '\n';
      return;
    }

    std::cout << "Starting local Valkey at " << uri << '\n';
    process = fork();
    if (process < 0)
      throw std::runtime_error("Failed to fork local Valkey process");
    if (process == 0)
    {
      const auto portString = std::to_string(port);
      execl(ATLASNET_LOCAL_VALKEY_SERVER, ATLASNET_LOCAL_VALKEY_SERVER,
            "--bind", "127.0.0.1", "--port", portString.c_str(), "--save", "",
            "--appendonly", "no", nullptr);
      _exit(127);
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
      if (IsValkeyReady(uri))
        return;

      int status = 0;
      if (waitpid(process, &status, WNOHANG) == process)
      {
        process = -1;
        throw std::runtime_error("Local Valkey exited during startup");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    kill(process, SIGTERM);
    waitpid(process, nullptr, 0);
    process = -1;
    throw std::runtime_error("Timed out waiting for local Valkey");
  }

  LocalValkey(const LocalValkey&) = delete;
  LocalValkey& operator=(const LocalValkey&) = delete;

  ~LocalValkey()
  {
    if (process <= 0)
      return;

    kill(process, SIGTERM);
    for (int attempt = 0; attempt < 100; ++attempt)
    {
      int status = 0;
      if (waitpid(process, &status, WNOHANG) == process)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(process, SIGKILL);
    waitpid(process, nullptr, 0);
  }

private:
  pid_t process = -1;
};
} // namespace

int main()
{
  try
  {
    constexpr uint16_t valkeyPort = ATLASNET_LOCAL_VALKEY_PORT;
    const std::string valkeyURI =
        "tcp://127.0.0.1:" + std::to_string(valkeyPort);
    LocalValkey valkey(valkeyURI, valkeyPort);

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    std::signal(SIGHUP, Stop);
    std::signal(SIGQUIT, Stop);

    AtlasNet::NodeConfig config;
    config.capabilities = AtlasNet::NodeCapability::Shard |
                          AtlasNet::NodeCapability::ClientIngress |
                          AtlasNet::NodeCapability::ControllerEligible |
                          AtlasNet::NodeCapability::Database;

    auto backend =
        std::make_unique<AtlasNet::DB::ValkeyRemoteBackend>(valkeyURI);
    AtlasNet::AtlasNetNode node(std::move(config), std::move(backend));
    node.Start();
    while (!stopRequested)
    {
      node.Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "AtlasNet local node: " << error.what() << '\n';
    return 1;
  }
}
