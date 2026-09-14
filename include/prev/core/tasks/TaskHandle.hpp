#pragma once

#include "taskflow/core/async_task.hpp"
#include <future>
namespace AtlasNet
{
template <typename ReturnType = void> class TaskHandle
{

std::shared_future<ReturnType> _future;
  tf::AsyncTask _task;

public:
  TaskHandle(std::pair<tf::AsyncTask, std::future<ReturnType>> task_pair)
      : _task(std::move(task_pair.first)), _future(task_pair.second.share())
  {
  }
  TaskHandle(std::pair<tf::AsyncTask, std::shared_future<ReturnType>> task_pair)
      : _task(std::move(task_pair.first)), _future(std::move(task_pair.second))
  {
  }
  TaskHandle(TaskHandle&&) = default;
  TaskHandle& operator=(TaskHandle&&) = default;
  TaskHandle(const TaskHandle&) = default;
  TaskHandle& operator=(const TaskHandle&) = default;

  std::shared_future<ReturnType>* operator->()
  {
    return &_future;
  }
  const std::shared_future<ReturnType>* operator->() const
  {
    return &_future;
  }


  tf::AsyncTask& GetTask()
  {
    return _task;
  }
  const tf::AsyncTask& GetTask() const
  {
    return _task;
  }
  operator tf::AsyncTask&()
  {
    return _task;
  }
  operator std::shared_future<ReturnType>&()
  {
    return _future;
  }
};
} // namespace AtlasNet