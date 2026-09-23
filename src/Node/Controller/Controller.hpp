#pragma once

#include <condition_variable>
#include <mutex>
#include <memory>
#include <spdlog/logger.h>
#include <stop_token>
#include <thread>

namespace AtlasNet
{
class Controller
{
public:
  explicit Controller(std::shared_ptr<spdlog::logger> logger)
      : logger(std::move(logger))
  {
  }
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  void Start();
  void Stop();

private:
  void Run(std::stop_token stopToken);

  // Runtime communication dependencies belong here. Handshake transport/RPC
  // must never be exposed to the controller.
  std::shared_ptr<spdlog::logger> logger;
  std::mutex lifecycleMutex;
  std::mutex runMutex;
  std::condition_variable_any wakeup;
  std::jthread thread;
};
} // namespace AtlasNet
