#pragma once

#include "JobHandle.hpp"
#include "JobOptions.hpp"
#include "atlasnet/core/job/JobContext.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace AtlasNet
{

class JobSystem final
{

public:
  struct Config
  {
    std::size_t threadCount = std::thread::hardware_concurrency();
  };
  using Clock = std::chrono::steady_clock;

  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;

  ~JobSystem()
  {
    Shutdown();
  }

  template <typename F, typename... Opts>
    requires(std::invocable<F&, JobContext&> &&
             (JobOpts::IsSupportedJobOptionV<Opts> && ...))
  [[nodiscard]] JobHandle Submit(F&& f, Opts&&... opts)
  {
    auto runtime = MakeRuntime(std::forward<F>(f), std::forward<Opts>(opts)...);
    EnqueueReady(runtime);
    return JobHandle(*this, runtime);
  }

  template <typename F, typename... Opts>
    requires(std::invocable<F&, JobContext&> &&
             (JobOpts::IsSupportedJobOptionV<Opts> && ...))
  [[nodiscard]] std::shared_ptr<Detail::JobRuntime> MakeRuntime(F&& f,
                                                                Opts&&... opts)
  {
    static_assert(JobOpts::CountIfV<JobOpts::IsName, Opts...> <= 1,
                  "Only one Name option is allowed");
    static_assert(JobOpts::CountIfV<JobOpts::IsPriority, Opts...> <= 1,
                  "Only one Priority option is allowed");
    static_assert(JobOpts::CountIfV<JobOpts::IsNotify, Opts...> <= 1,
                  "Only one Notify option is allowed");
    static_assert(JobOpts::CountIfV<JobOpts::IsRepeat, Opts...> <= 1,
                  "Only one Repeat option is allowed");

    auto runtime = std::make_shared<Detail::JobRuntime>();
    ConfigureRuntime(*runtime, std::forward<F>(f), std::forward<Opts>(opts)...);
    return runtime;
  }

  void EnqueueFromHandle(const std::shared_ptr<Detail::JobRuntime>& runtime)
  {
    EnqueueReady(runtime);
  }

  void
  EnqueueRepeatFromHandle(const std::shared_ptr<Detail::JobRuntime>& runtime,
                          std::chrono::milliseconds delay)
  {
    EnqueueDelayed(runtime, delay);
  }

  void Shutdown()
{
  {
    std::lock_guard lock(mutex_);
    if (!started_)
      return;

    stopRequested_ = true;
  }

  cv_.notify_all();
std::cerr << "Shutdown requested, waiting for worker threads to finish..."
            << std::endl;
  for (auto& worker : workers_)
  {
    if (worker.joinable())
    {
      if (worker.get_id() == std::this_thread::get_id())
        continue; // or std::terminate/assert, depending on your design
      worker.join();
    }
  }
  std::cerr << "All worker threads have finished." << std::endl;

  workers_.clear();

  {
    std::lock_guard lock(mutex_);
    started_ = false;

    while (!ready_.empty())
      ready_.pop();

    while (!delayed_.empty())
      delayed_.pop();
  }
}

public:
  JobSystem(const Config& config) : _config(config)
  {
    const std::size_t threadCount = _config.threadCount;

    started_ = true;
    stopRequested_ = false;

    workers_.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i)
    {
      workers_.emplace_back([this] { WorkerLoop(); });
    }
  }

private:
  std::uint64_t NextSequence()
  {
    return sequence_.fetch_add(1, std::memory_order_relaxed);
  }

  void EnqueueReady(const std::shared_ptr<Detail::JobRuntime>& runtime)
  {
    {
      std::lock_guard lock(mutex_);

      if (!started_)
      {
        throw std::runtime_error(
            "JobSystem is not running. Call JobSystem::Init(...) first.");
      }

      runtime->state.store(JobState::eQueued, std::memory_order_release);
      ready_.push(Detail::ReadyJob{runtime, NextSequence()});
    }
    cv_.notify_one();
  }

  void EnqueueDelayed(const std::shared_ptr<Detail::JobRuntime>& runtime,
                      std::chrono::milliseconds delay)
  {
    {
      std::lock_guard lock(mutex_);

      if (!started_)
      {
        throw std::runtime_error(
            "JobSystem is not running. Call JobSystem::Init(...) first.");
      }

      runtime->state.store(JobState::ePending, std::memory_order_release);
      delayed_.push(
          Detail::DelayedJob{Clock::now() + delay, runtime, NextSequence()});
    }
    cv_.notify_one();
  }

  void PromoteExpiredDelayedJobsLocked()
  {
    const auto now = Clock::now();

    while (!delayed_.empty() && delayed_.top().due <= now)
    {
      auto delayedJob = delayed_.top();
      delayed_.pop();

      if (delayedJob.runtime->state.load(std::memory_order_acquire) ==
          JobState::eCancelled)
      {
        continue;
      }

      delayedJob.runtime->state.store(JobState::eQueued,
                                      std::memory_order_release);
      ready_.push(Detail::ReadyJob{delayedJob.runtime, NextSequence()});
    }
  }

  template <typename F, typename... Opts>
  void ConfigureRuntime(Detail::JobRuntime& runtime, F&& f, Opts&&... opts)
  {
    runtime.invoke = [fn = std::forward<F>(f)](JobContext& ctx) mutable
    { std::invoke(fn, ctx); };

    (ApplyOption(runtime, std::forward<Opts>(opts)), ...);
  }

  void ApplyOption(Detail::JobRuntime& runtime, JobOpts::Name name)
  {
    runtime.name = std::move(name.value);
  }

  void ApplyOption(Detail::JobRuntime& runtime, JobOpts::RepeatOnce repeat)
  {
    runtime.repeat_requested = true;
    runtime.repeat_delay = repeat.interval;
  }

  template <JobPriority P>
  void ApplyOption(Detail::JobRuntime& runtime, JobOpts::TPriority<P>)
  {
    runtime.priority = P;
  }
  void ApplyOption(Detail::JobRuntime& runtime, JobOpts::Priority priority)
  {
    runtime.priority = priority.value;
  }

  template <JobNotifyLevel N>
  void ApplyOption(Detail::JobRuntime& runtime, JobOpts::Notify<N>)
  {
    runtime.notify = N;
  }

  template <typename F, typename... Opts>
  void ApplyOption(Detail::JobRuntime& runtime,
                   JobOpts::OnCompleteT<F, Opts...>&& child)
  {
    runtime.continuations.push_back(Detail::JobContinuationFactory{
        [this, child = std::move(child)]() mutable
        {
          auto childRuntime = std::make_shared<Detail::JobRuntime>();
          ConfigureContinuationRuntime(*childRuntime, std::move(child));
          return childRuntime;
        }});
  }

  template <typename F, typename... Opts>
  void ApplyOption(Detail::JobRuntime& runtime,
                   const JobOpts::OnCompleteT<F, Opts...>& child)
  {
    runtime.continuations.push_back(Detail::JobContinuationFactory{
        [this, child]()
        {
          auto childRuntime = std::make_shared<Detail::JobRuntime>();
          ConfigureContinuationRuntime(*childRuntime, child);
          return childRuntime;
        }});
  }

  template <typename F, typename... Opts>
  void ConfigureContinuationRuntime(Detail::JobRuntime& runtime,
                                    JobOpts::OnCompleteT<F, Opts...>&& child)
  {
    runtime.invoke = [fn = std::move(child.func)](JobContext& ctx) mutable
    { std::invoke(fn, ctx); };

    std::apply(
        [&](auto&&... childOpts)
        {
          (ApplyOption(runtime, std::forward<decltype(childOpts)>(childOpts)),
           ...);
        },
        std::move(child.opts));
  }

  template <typename F, typename... Opts>
  void
  ConfigureContinuationRuntime(Detail::JobRuntime& runtime,
                               const JobOpts::OnCompleteT<F, Opts...>& child)
  {
    runtime.invoke = [fn = child.func](JobContext& ctx) mutable
    { std::invoke(fn, ctx); };

    std::apply([&](const auto&... childOpts)
               { (ApplyOption(runtime, childOpts), ...); }, child.opts);
  }

  void WorkerLoop()
  {
    for (;;)
    {
      std::shared_ptr<Detail::JobRuntime> runtime;

      {
        std::unique_lock lock(mutex_);

        for (;;)
        {
          PromoteExpiredDelayedJobsLocked();

          if (stopRequested_ && ready_.empty() && delayed_.empty())
            return;

          if (!ready_.empty())
          {
            runtime = ready_.top().runtime;
            ready_.pop();
            break;
          }

          if (!delayed_.empty())
          {
            cv_.wait_until(lock, delayed_.top().due);
          }
          else
          {
            cv_.wait(lock);
          }
        }
      }

      Execute(runtime);
    }
  }

  void Execute(const std::shared_ptr<Detail::JobRuntime>& runtime)
  {
    {
      std::lock_guard lock(runtime->mutex);

      const auto s = runtime->state.load(std::memory_order_acquire);
      if (s == JobState::eCancelled || runtime->cancelled)
      {
        runtime->cv.notify_all();
        return;
      }

      runtime->state.store(JobState::eRunning, std::memory_order_release);
    }

    try
    {
      const auto threadId =
          std::hash<std::thread::id>{}(std::this_thread::get_id());

      if (HasFlag(runtime->notify, JobNotifyLevel::eOnStart))
      {
        std::cout << std::format("Starting job '{}' in thread {}",
                                 runtime->name.value_or("<unnamed>"), threadId)
                  << std::endl;
      }

      JobContext ctx(*runtime);
      runtime->invoke(ctx);

      bool shouldRepeat = false;
      std::chrono::milliseconds repeatDelay{0};

      {
        std::lock_guard lock(runtime->mutex);

        shouldRepeat = runtime->repeat_requested && !runtime->cancelled;
        repeatDelay = runtime->repeat_delay;

        runtime->repeat_requested = false;
        runtime->repeat_delay = std::chrono::milliseconds::zero();

        if (shouldRepeat)
        {
          runtime->state.store(JobState::ePending, std::memory_order_release);
        }
        else
        {
          runtime->state.store(JobState::eCompleted, std::memory_order_release);
        }
      }

      if (shouldRepeat)
      {
        EnqueueDelayed(runtime, repeatDelay);
        return;
      }

      runtime->cv.notify_all();

      if (HasFlag(runtime->notify, JobNotifyLevel::eOnComplete))
      {
        std::cout << std::format("Completed job '{}' in thread {}",
                                 runtime->name.value_or("<unnamed>"), threadId)
                  << std::endl;
      }

      std::vector<Detail::JobContinuationFactory> continuationsToRun;
      {
        std::lock_guard lock(runtime->mutex);
        continuationsToRun = runtime->continuations;
      }

      for (auto& factory : continuationsToRun)
      {
        if (factory.create)
        {
          auto next = factory.create();
          EnqueueReady(next);
        }
      }
    }
    catch (...)
    {
      {
        std::lock_guard lock(runtime->mutex);
        runtime->exception = std::current_exception();
        runtime->state.store(JobState::eFailed, std::memory_order_release);
      }
      runtime->cv.notify_all();

      if (HasFlag(runtime->notify, JobNotifyLevel::eOnFailure))
      {
      }
    }
  }

private:
  const Config _config;
  std::mutex mutex_;
  std::condition_variable cv_;

  std::vector<std::thread> workers_;

  std::priority_queue<Detail::ReadyJob, std::vector<Detail::ReadyJob>,
                      Detail::ReadyJobCompare>
      ready_;

  std::priority_queue<Detail::DelayedJob, std::vector<Detail::DelayedJob>,
                      Detail::DelayedJobCompare>
      delayed_;

  std::atomic<std::uint64_t> sequence_{0};

  bool started_ = false;
  bool stopRequested_ = false;
};

inline void JobHandle::request_repeat(std::chrono::milliseconds delay) const
{
  if (!runtime_ || !system_)
    return;

  bool enqueueNow = false;

  {
    std::lock_guard lock(runtime_->mutex);

    const auto s = runtime_->state.load(std::memory_order_acquire);
    if (s == JobState::eFailed || s == JobState::eCancelled)
      return;

    if (s == JobState::eCompleted)
    {
      enqueueNow = true;
    }
    else
    {
      runtime_->repeat_requested = true;
      runtime_->repeat_delay = delay;
    }
  }

  if (enqueueNow)
  {
    system_->EnqueueRepeatFromHandle(runtime_, delay);
  }
}

template <typename F, typename... Opts>
  requires(std::invocable<F&, JobContext&> &&
           (JobOpts::IsSupportedJobOptionV<Opts> && ...))
JobHandle JobHandle::on_complete(F&& f, Opts&&... opts) const
{
  using FactoryTuple = std::tuple<std::decay_t<F>, std::decay_t<Opts>...>;

  auto factoryData = std::make_shared<FactoryTuple>(
      std::forward<F>(f), std::forward<Opts>(opts)...);

  JobSystem* system = system_;
  auto makeRuntime = [system,
                      factoryData]() -> std::shared_ptr<Detail::JobRuntime>
  {
    return std::apply([&](auto& fn, auto&... options)
                      { return system->MakeRuntime(fn, options...); },
                      *factoryData);
  };

  if (!runtime_ || !system)
  {
    if (!system)
      return JobHandle{};

    auto firstRuntime = makeRuntime();
    system->EnqueueFromHandle(firstRuntime);
    return JobHandle(*system, firstRuntime);
  }

  auto firstRuntime = makeRuntime();

  bool submitNow = false;
  bool doNothing = false;

  {
    std::lock_guard lock(runtime_->mutex);
    const auto s = runtime_->state.load(std::memory_order_acquire);

    if (s == JobState::eCompleted)
    {
      submitNow = true;
    }
    else if (s == JobState::eFailed || s == JobState::eCancelled)
    {
      doNothing = true;
    }
    else
    {
      runtime_->continuations.push_back(
          Detail::JobContinuationFactory{makeRuntime});
    }
  }

  if (submitNow)
  {
    system->EnqueueFromHandle(firstRuntime);
    return JobHandle(*system, firstRuntime);
  }

  if (doNothing)
  {
    return JobHandle{};
  }

  return JobHandle(*system, firstRuntime);
}

} // namespace AtlasNet