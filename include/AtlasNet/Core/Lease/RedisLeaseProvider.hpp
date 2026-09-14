#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Lease/ILeaseProvider.hpp"
#include <chrono>
#include <future>
#include <sw/redis++/redis.h>
namespace AtlasNet
{
class RedisLease : public ILease
{

public:
  RedisLease(ILeaseProvider* provider, AtlasNetNodeID nodeId,
             std::string resource,
             std::shared_ptr<sw::redis::Redis> redisClient)
      : ILease(provider, resource, nodeId), m_redisClient(redisClient)
  {
  }
  void Renew(std::chrono::milliseconds duration) override
  {
    std::string renewLua = R"(
      local key = KEYS[1]
      local value = ARGV[1]
      local duration = tonumber(ARGV[2])
      local currentValue = redis.call("GET", key)
      if currentValue == value then
          redis.call("PEXPIRE", key, duration)
          return 1
      else
          return 0
      end)";
    std::string commands[] = {"EVAL",
                              renewLua,
                              "1",
                              GetResource(),
                              GetOwner().to_string(),
                              std::to_string(duration.count())};
    const auto result = m_redisClient->command<long long>(std::begin(commands),
                                                          std::end(commands));
    if (result != 1)
    {
      logger->error("Failed to renew lease for resource: {}", GetResource());
    }
  }

  void Release() override
  {
    std::string renewLua = R"(
      local key = KEYS[1]
      local value = ARGV[1]
      local currentValue = redis.call("GET", key)
      if redis.call("GET", key) == value then
    return redis.call("DEL", key)
end

return 0
      )";
    std::string commands[] = {"EVAL", renewLua, "1", GetResource(),
                              GetOwner().to_string()};
    const auto result = m_redisClient->command<long long>(std::begin(commands),
                                                          std::end(commands));
    if (result != 1)
    {
      logger->error("Failed to release lease for resource: {}", GetResource());
    }
  }

private:
  const static inline std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("RedisLease");
  std::shared_ptr<sw::redis::Redis> m_redisClient;
};
class RedisLeaseProvider : public ILeaseProvider
{
  std::shared_ptr<sw::redis::Redis> m_redisClient;

public:
  RedisLeaseProvider(AtlasNetNodeID nodeId,
                     std::shared_ptr<sw::redis::Redis> redisClient)
      : ILeaseProvider(nodeId), m_redisClient(redisClient)
  {
  }
  std::expected<std::unique_ptr<ILease>, LeaseClaimError>
  Claim(std::string_view resource) override
  {
    const bool KeySet = m_redisClient->set(
        resource, GetOwner().to_string(),
        std::chrono::milliseconds(GetLeaseDuration().count()),
        sw::redis::UpdateType::NOT_EXIST);
    if (KeySet)
    {
      return std::expected<std::unique_ptr<ILease>, LeaseClaimError>(
          std::make_unique<RedisLease>(this, GetOwner(), std::string(resource),
                                       m_redisClient));
    }
    else
    {
      return std::expected<std::unique_ptr<ILease>, LeaseClaimError>(
          std::unexpected(LeaseClaimError::AlreadyClaimed));
    }
  }

  std::optional<LeaseInfo>
  GetActiveLease(std::string_view resource) const override
  {
    const auto val = m_redisClient->get(resource);
    if (val)
    {
      return LeaseInfo{AtlasNetNodeID::from_string(*val)};
    }
    return std::nullopt;
  }

  std::future<LeaseClaimResult>
  ClaimOrGetLeaseAsync(std::string_view resource,
                       std::chrono::milliseconds timeout) override
  {

    return std::async(
        std::launch::async,
        [this, resource, timeout]()
        {
          std::chrono::milliseconds elapsedTime(0);

          while (elapsedTime < timeout)
          {
            std::chrono::system_clock::time_point startTime =
                std::chrono::system_clock::now();
            auto claimResult = Claim(resource);
            if (claimResult)
            {
              return LeaseClaimResult(std::move(claimResult.value()));
            }
            else
            {
              auto activeLease = GetActiveLease(resource);
              if (activeLease)
              {
                return LeaseClaimResult(std::move(activeLease.value()));
              }
            }
            elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - startTime);
          }
          return LeaseClaimResult(
              std::unexpected(LeaseClaimError::LeaseServiceUnavailable));
        });
  }

private:
  constexpr std::chrono::milliseconds GetLeaseDuration() const override
  {
    return std::chrono::milliseconds(10000); // Example duration
  }

  constexpr std::chrono::milliseconds GetLeaseRenewalDuration() const override
  {
    return std::chrono::milliseconds(5000); // Example renewal duration
  }
};
} // namespace AtlasNet