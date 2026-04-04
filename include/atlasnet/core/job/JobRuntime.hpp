#pragma once

#include "JobEnums.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace AtlasNet
{

class JobContext;

namespace Detail
{

struct JobRuntime;

struct JobContinuationFactory
{
  std::function<std::shared_ptr<JobRuntime>()> create;
};

struct JobRuntime
{
  using InvokeFn = std::function<void(JobContext&)>;

  mutable std::mutex mutex;
  std::condition_variable cv;

  std::atomic<JobState> state{JobState::ePending};

  JobPriority priority = JobPriority::eMedium;
  JobNotifyLevel notify = JobNotifyLevel::eNone;
  std::optional<std::string> name;

  bool repeat_requested = false;
  std::chrono::milliseconds repeat_delay{0};

  bool cancelled = false;
  std::exception_ptr exception;

  std::vector<JobContinuationFactory> continuations;

  InvokeFn invoke;
};

struct ReadyJob
{
  std::shared_ptr<JobRuntime> runtime;
  std::uint64_t sequence = 0;
};

struct ReadyJobCompare
{
  bool operator()(const ReadyJob& a, const ReadyJob& b) const noexcept
  {
    const auto pa = static_cast<int>(a.runtime->priority);
    const auto pb = static_cast<int>(b.runtime->priority);

    if (pa != pb)
      return pa < pb;

    return a.sequence > b.sequence;
  }
};

struct DelayedJob
{
  std::chrono::steady_clock::time_point due;
  std::shared_ptr<JobRuntime> runtime;
  std::uint64_t sequence = 0;
};

struct DelayedJobCompare
{
  bool operator()(const DelayedJob& a, const DelayedJob& b) const noexcept
  {
    if (a.due != b.due)
      return a.due > b.due;

    return a.sequence > b.sequence;
  }
};

} // namespace Detail
} // namespace AtlasNet