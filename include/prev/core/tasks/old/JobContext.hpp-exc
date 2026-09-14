#pragma once

#include "JobRuntime.hpp"
#include <chrono>
#include <string_view>

namespace AtlasNet
{

class JobContext
{
public:
  explicit JobContext(Detail::JobRuntime& runtime) noexcept
      : runtime_(runtime)
  {
  }

  [[nodiscard]] JobPriority priority() const noexcept
  {
    return runtime_.priority;
  }

  [[nodiscard]] JobNotifyLevel notify_level() const noexcept
  {
    return runtime_.notify;
  }

  [[nodiscard]] std::string_view name() const noexcept
  {
    if (!runtime_.name.has_value())
      return {};
    return *runtime_.name;
  }

  JobContext& set_repeat_once(
      std::chrono::milliseconds delay = std::chrono::milliseconds::zero()) noexcept
  {
    std::lock_guard lock(runtime_.mutex);
    runtime_.repeat_requested = true;
    runtime_.repeat_delay = delay;
    return *this;
  }

  [[nodiscard]] bool is_cancelled() const noexcept
  {
    std::lock_guard lock(runtime_.mutex);
    return runtime_.cancelled;
  }
  /* void cancel() noexcept
  {
    std::lock_guard lock(runtime_.mutex);
    runtime_.cancelled = true;
    runtime_.state.store(JobState::eCancelled, std::memory_order_release);
    runtime_.cv.notify_all();
  } */

  void set_failure() noexcept
  {
    std::lock_guard lock(runtime_.mutex);
    runtime_.state.store(JobState::eFailed, std::memory_order_release);
    runtime_.cv.notify_all();
  }

private:
  Detail::JobRuntime& runtime_;
};

} // namespace AtlasNet
