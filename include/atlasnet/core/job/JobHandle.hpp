#pragma once

#include "JobContext.hpp"
#include "JobOptions.hpp"
#include "JobRuntime.hpp"
#include <chrono>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace AtlasNet
{

class JobSystem;

class JobHandle
{
public:
  JobHandle() = default;

  JobHandle(JobSystem& system, std::shared_ptr<Detail::JobRuntime> runtime)
      : system_(&system), runtime_(std::move(runtime))
  {
  }

  [[nodiscard]] bool valid() const noexcept
  {
    return static_cast<bool>(runtime_);
  }

  [[nodiscard]] JobState state() const noexcept
  {
    if (!runtime_)
      return JobState::eCompleted;
    return runtime_->state.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool is_completed() const noexcept
  {
    return state() == JobState::eCompleted;
  }

  [[nodiscard]] bool is_failed() const noexcept
  {
    return state() == JobState::eFailed;
  }

  [[nodiscard]] bool is_running() const noexcept
  {
    return state() == JobState::eRunning;
  }

  [[nodiscard]] bool is_pending() const noexcept
  {
    const auto s = state();
    return s == JobState::ePending || s == JobState::eQueued;
  }

  void wait() const
  {
    if (!runtime_)
      return;

    std::unique_lock lock(runtime_->mutex);
    runtime_->cv.wait(lock,
                      [&]
                      {
                        const auto s =
                            runtime_->state.load(std::memory_order_acquire);
                        return s == JobState::eCompleted ||
                               s == JobState::eFailed ||
                               s == JobState::eCancelled;
                      });
  }

  void rethrow_if_failed() const
  {
    if (!runtime_)
      return;

    std::exception_ptr ex;
    {
      std::lock_guard lock(runtime_->mutex);
      ex = runtime_->exception;
    }
    if (ex)
      std::rethrow_exception(ex);
  }

  [[nodiscard]] std::optional<std::string> name() const
  {
    if (!runtime_)
      return std::nullopt;

    std::lock_guard lock(runtime_->mutex);
    return runtime_->name;
  }

  [[nodiscard]] JobPriority priority() const noexcept
  {
    if (!runtime_)
      return JobPriority::eMedium;
    return runtime_->priority;
  }

  void request_repeat(
      std::chrono::milliseconds delay = std::chrono::milliseconds::zero()) const;

  void cancel() const noexcept
  {
    if (!runtime_)
      return;

    std::lock_guard lock(runtime_->mutex);
    const auto s = runtime_->state.load(std::memory_order_acquire);
    if (s == JobState::ePending || s == JobState::eQueued)
    {
      runtime_->cancelled = true;
      runtime_->state.store(JobState::eCancelled, std::memory_order_release);
      runtime_->cv.notify_all();
    }
  }

  template <typename F, typename... Opts>
    requires(std::invocable<F&, JobContext&> &&
             (JobOpts::IsSupportedJobOptionV<Opts> && ...))
  [[nodiscard]] JobHandle on_complete(F&& f, Opts&&... opts) const;

private:
  JobSystem* system_ = nullptr;
  std::shared_ptr<Detail::JobRuntime> runtime_;
};

} // namespace AtlasNet
