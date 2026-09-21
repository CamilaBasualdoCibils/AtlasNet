#include "Backends/ValkeyRemoteBackend.hpp"
#include "Configuration.hpp"
#include <csignal>
#include <iostream>

namespace
{
volatile std::sig_atomic_t stopRequested = 0;
void Stop(int)
{
  stopRequested = 1;
}
} // namespace

int main(int argc, char** argv)
{
  try
  {
    auto config = AtlasNet::Standalone::ParseConfiguration(argc, argv);
    if (config.help)
      return 0;
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    std::signal(SIGHUP, Stop);
    std::signal(SIGQUIT, Stop);
    std::unique_ptr<AtlasNet::DB::IDatabaseBackend> backend;
    if (AtlasNet::HasCapability(config.node.capabilities,
                                AtlasNet::NodeCapability::Database))
      backend =
          std::make_unique<AtlasNet::DB::ValkeyRemoteBackend>(config.valkeyURI);
    AtlasNet::AtlasNetNode node(std::move(config.node), std::move(backend));
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
    std::cerr << "AtlasNet: " << error.what() << '\n';
    return 1;
  }
}
