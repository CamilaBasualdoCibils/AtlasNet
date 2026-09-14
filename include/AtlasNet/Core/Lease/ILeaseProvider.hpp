#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <chrono>
#include <expected>
#include <future>
#include <memory>
#include <thread>
namespace AtlasNet
{
enum class LeaseClaimError
{
  AlreadyClaimed,          // Another owner currently holds the lease.
  LeaseServiceUnavailable, // Redis/etcd/etc. couldn't be reached.
  InvalidResource,         // Resource name doesn't exist or is malformed.
  PermissionDenied,        // Caller is not allowed to claim this resource.
  InternalError
};
enum class LeaseStatus
{
  Active,   // Lease is currently held by an owner.
  Expired,  // Lease has expired and is no longer valid.
  Released, // Lease has been released by the owner.
  Unknown   // Lease status cannot be determined.
};
struct LeaseInfo
{
  AtlasNetNodeID Owner;
};
class ILeaseProvider;

class ILease;
class ILeaseProvider
{
  friend class ILease;

public:
  ILeaseProvider(AtlasNetNodeID nodeId) : m_nodeId(nodeId) {}
  virtual ~ILeaseProvider() = default;

public:
  virtual std::expected<std::unique_ptr<ILease>, LeaseClaimError>
  Claim(std::string_view resource) = 0;

  virtual std::optional<LeaseInfo>
  GetActiveLease(std::string_view resource) const = 0;

  using LeaseClaimResult =
      std::expected<std::variant<std::unique_ptr<ILease>, LeaseInfo>,
                    LeaseClaimError>;

  [[nodiscard]] virtual std::future<LeaseClaimResult> ClaimOrGetLeaseAsync(
      std::string_view resource,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) = 0;
  AtlasNetNodeID GetOwner() const
  {
    return m_nodeId;
  }

private:
  virtual std::chrono::milliseconds GetLeaseDuration() const = 0;
  virtual std::chrono::milliseconds GetLeaseRenewalDuration() const = 0;

protected:
private:
  AtlasNetNodeID m_nodeId;
};
class ILease
{
public:
  ILease(ILeaseProvider* provider, std::string resource, AtlasNetNodeID nodeId)
      : m_provider(provider), m_resource(resource), m_nodeId(nodeId)
  {
    logger->info("Lease created for resource: {} by owner: {}",
                 m_resource, GetOwner().to_string());
    m_renewalThread = std::jthread(
        [this](std::stop_token stopToken)
        {
          while (!stopToken.stop_requested())
          {
            std::this_thread::sleep_for(m_provider->GetLeaseRenewalDuration());
            Renew(m_provider->GetLeaseDuration());
          }
          logger->info("Releasing lease for resource: {}", m_resource);
          Release();
        });
  }
  virtual ~ILease()
  {

    m_renewalThread.request_stop();
    m_renewalThread.join();
  }

  virtual void Renew(std::chrono::milliseconds duration) = 0;
  virtual void Release() = 0;
  std::string GetResource() const
  {
    return m_resource;
  }

  AtlasNetNodeID GetOwner() const
  {
    return m_nodeId;
  }

private:
  const static inline std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("ILease");
  std::jthread m_renewalThread;
  ILeaseProvider* m_provider;
  std::string m_resource;
  AtlasNetNodeID m_nodeId;
};
} // namespace AtlasNet