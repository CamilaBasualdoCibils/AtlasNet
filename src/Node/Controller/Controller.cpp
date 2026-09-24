#include "Controller.hpp"

void AtlasNet::Controller::Start()
{
  std::scoped_lock lock(lifecycleMutex);
  if (thread.joinable())
    return;

  thread = std::jthread(
      [this](std::stop_token stopToken) { Run(std::move(stopToken)); });
}

void AtlasNet::Controller::Stop()
{
  std::scoped_lock lock(lifecycleMutex);
  if (!thread.joinable())
    return;

  thread.request_stop();
  wakeup.notify_all();
  thread.join();
}

void AtlasNet::Controller::Run(std::stop_token stopToken)
{
  logger->info("Controller started");

  // Controller orchestration belongs on this execution context. Work can be
  // dispatched from here without blocking the node's network/polling loop.
  std::unique_lock lock(runMutex);
  wakeup.wait(lock, stopToken, [] { return false; });

  logger->info("Controller stopped");
}

AtlasNet::Controller::~Controller()
{
  Stop();
}
