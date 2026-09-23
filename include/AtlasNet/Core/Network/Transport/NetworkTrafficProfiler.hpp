#pragma once

#ifdef ATLASNET_TRACY_ENABLED

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace AtlasNet::Network
{
class NetworkTrafficProfiler
{
public:
  struct Rates
  {
    double txBytesPerSecond;
    double rxBytesPerSecond;
    double txPacketsPerSecond;
    double rxPacketsPerSecond;
  };

  void RecordTx(size_t bytes)
  {
    std::scoped_lock lock(mutex_);
    txBytes_ += bytes;
    ++txPackets_;
  }

  void RecordRx(size_t bytes)
  {
    std::scoped_lock lock(mutex_);
    rxBytes_ += bytes;
    ++rxPackets_;
  }

  std::optional<Rates> Sample()
  {
    const auto now = Clock::now();
    std::scoped_lock lock(mutex_);
    const std::chrono::duration<double> elapsed = now - sampledAt_;
    if (elapsed < SampleInterval)
      return std::nullopt;

    const double seconds = elapsed.count();
    const Rates rates{
        .txBytesPerSecond = static_cast<double>(txBytes_) / seconds,
        .rxBytesPerSecond = static_cast<double>(rxBytes_) / seconds,
        .txPacketsPerSecond = static_cast<double>(txPackets_) / seconds,
        .rxPacketsPerSecond = static_cast<double>(rxPackets_) / seconds,
    };
    txBytes_ = 0;
    rxBytes_ = 0;
    txPackets_ = 0;
    rxPackets_ = 0;
    sampledAt_ = now;
    return rates;
  }

private:
  using Clock = std::chrono::steady_clock;
  static constexpr std::chrono::seconds SampleInterval{1};

  std::mutex mutex_;
  Clock::time_point sampledAt_ = Clock::now();
  uint64_t txBytes_ = 0;
  uint64_t rxBytes_ = 0;
  uint64_t txPackets_ = 0;
  uint64_t rxPackets_ = 0;
};
} // namespace AtlasNet::Network

#endif
