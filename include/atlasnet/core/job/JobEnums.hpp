#pragma once

#include <cstdint>
#include <boost/describe.hpp>
namespace AtlasNet
{

enum class JobPriority : std::uint8_t
{
  eLowest = 0,
  eLow = 1,
  eMedium = 2,
  eHigh = 3,
  eHighest = 4
};

enum class JobNotifyLevel : std::uint8_t
{
  eNone = 0,
  eOnStart = 1 << 0,
  eOnComplete = 1 << 1,
  eOnFailure = 1 << 2,


  eOnStartAndComplete = eOnStart | eOnComplete,
  eOnAll = eOnStart | eOnComplete | eOnFailure
};

constexpr JobNotifyLevel operator|(JobNotifyLevel a,
                                   JobNotifyLevel b) noexcept
{
  return static_cast<JobNotifyLevel>(static_cast<std::uint8_t>(a) |
                                     static_cast<std::uint8_t>(b));
}

constexpr JobNotifyLevel operator&(JobNotifyLevel a,
                                   JobNotifyLevel b) noexcept
{
  return static_cast<JobNotifyLevel>(static_cast<std::uint8_t>(a) &
                                     static_cast<std::uint8_t>(b));
}

constexpr bool HasFlag(JobNotifyLevel value, JobNotifyLevel flag) noexcept
{
  return static_cast<std::uint8_t>(value & flag) != 0;
}

enum class JobState : std::uint8_t
{
  ePending,
  eQueued,
  eRunning,
  eCompleted,
  eFailed,
  eCancelled
};
BOOST_DESCRIBE_ENUM(JobState, ePending, eQueued, eRunning, eCompleted, eFailed, eCancelled)

} // namespace AtlasNet