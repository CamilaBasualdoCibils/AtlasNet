#pragma once

#include "IEvent.hpp"
#include "atlasnet/core/events/IEventSystem.hpp"

#include "atlasnet/core/tasks/TaskHandle.hpp"
#include "atlasnet/core/tasks/TaskSystem.hpp"
#include "enviroment/Enviroment.hpp"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
namespace AtlasNet
{
class LocalEventSystem : public IEventSystem
{
public:
  struct Config
  {
    TaskSystem* taskSystem;
  };
  explicit LocalEventSystem(const Config& config)
      : taskSystem(config.taskSystem)
  {
  }

protected:
  void impl_On(EventID eventID,
               std::function<void(const std::string_view&)> cb) override
  {
    logger->info("Registering Local listener for event ID {}", eventID);
    std::unique_lock lock(mutex);
    listeners[eventID].push_back(std::move(cb));
  }

  TaskHandle<> impl_Emit(EventID eventID,
                         const std::string_view& event) override
  {
    TaskHandle<> handle = taskSystem->MediumPriority().dependent_async(
        [this, eventID, eventstr = std::string(event)]()
        {
          std::vector<std::function<void(const std::string_view&)>> callbacks;

          {
            std::shared_lock lock(mutex);

            auto it = listeners.find(eventID);
            if (it == listeners.end())
              return;

            callbacks = it->second; // COPY OUT
          }

          // lock is released here

          for (auto& cb : callbacks)
          {
            cb(eventstr);
          }
        });
    return handle;
  }

private:
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("LocalEventSystem");
  TaskSystem* taskSystem;

  std::shared_mutex mutex;
  std::unordered_map<EventID,
                     std::vector<std::function<void(const std::string_view&)>>>
      listeners;
};
} // namespace AtlasNet
