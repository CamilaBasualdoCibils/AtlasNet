#pragma once
#include "taskflow/core/async_task.hpp"
#include "taskflow/core/executor.hpp"
#include "taskflow/core/task.hpp"
#include <cstddef>
#include <taskflow/taskflow.hpp>

namespace AtlasNet
{
class TaskSystem
{
  tf::Executor Executor;

public:
  struct Config
  {
    std::size_t highPriorityThreadCount = std::thread::hardware_concurrency();
    std::size_t lowPriorityThreadCount = std::thread::hardware_concurrency()/2;
  };
  TaskSystem(const Config& config) : Executor(config.highPriorityThreadCount)
  {
  }
  void Shutdown()
  {
    Executor.wait_for_all();
  }

  tf::Executor& HighPriority()
  {
    return Executor;
  }
  tf::Executor& LowPriority()
  {
    return Executor;
  }
  tf::Executor& MediumPriority()
  {
    return Executor;
  }


  ~TaskSystem() = default;
};
} // namespace AtlasNet
