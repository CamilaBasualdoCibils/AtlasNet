#pragma once

#include "AtlasNet/Core/Events/IGlobalEvents.hpp"
#include "AtlasNet/Node/NodeData.hpp"
#include "Node/ServiceDiscovery/IServiceDiscovery.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <memory>
#include <sw/redis++/async_redis++.h>
#include <sw/redis++/async_subscriber.h>
#include <sw/redis++/command_options.h>
#include <sw/redis++/redis++.h>
#include <sw/redis++/subscriber.h>
#include <thread>
namespace AtlasNet::Service
{
class RedisServiceLease : public IServiceLease
{
public:
  RedisServiceLease(std::shared_ptr<sw::redis::Redis> redisClient,
                    const std::string& serviceKey,
                    const std::string& serviceField,
                    std::chrono::milliseconds ttl,
                    std::chrono::milliseconds heartbeatInterval)
      : redisClient(redisClient), serviceKey(serviceKey)
  {
    heartbeatThread = std::jthread(
        [this, ttl, heartbeatInterval, serviceKey = serviceKey,
         serviceField = serviceField,
         redisClient = redisClient](std::stop_token st)
        {
          while (!st.stop_requested())
          {
            std::this_thread::sleep_for(heartbeatInterval);
            /* std::string field[1] = {serviceField};
            redisClient->hsetex(serviceKey, std::begin(field), std::end(field),
                                ttl, sw::redis::HSetExOption::ALWAYS);
            std::string sec_str = std::to_string(seconds); */

            std::string command[] = {
                "HPEXPIRE", serviceKey, std::to_string(ttl.count()),
                "FIELDS",   "1",        serviceField};
          }
        });
  }
  std::jthread heartbeatThread;
  ~RedisServiceLease()
  {
    if (heartbeatThread.joinable())
    {
      heartbeatThread.request_stop();
      heartbeatThread.join();
    }
  }

private:
  std::shared_ptr<sw::redis::Redis> redisClient;
  std::string serviceKey;
};
class RedisServiceDiscovery : public IServiceDiscovery
{
public:
  RedisServiceDiscovery(std::shared_ptr<sw::redis::Redis> redisClient)
      : redisClient(redisClient)
  {
  }
  ~RedisServiceDiscovery() {}
  [[nodiscard]] std::unique_ptr<IServiceLease>
  RegisterService(const NodeData& data) override
  {
    std::pair<std::string, std::string> values[] = {
        {data.nodeID.to_string(), data.to_json().dump()}};

    const bool result = redisClient->hsetex(
        ServiceRegistryKey, std::begin(values), std::end(values),
        std::chrono::milliseconds(2000), sw::redis::HSetExOption::ALWAYS);
    if (!result)
    {
      logger->error("Failed to register service in Redis");
      throw std::runtime_error("Failed to register service in Redis");
    }
    /*     redisClient->hset(ServiceRegistryKey,
                          {data.nodeID.to_string(), data.to_json().dump(4)}); */
    logger->info("Registered Node:\n{}", data.to_json().dump(4));

    return std::make_unique<RedisServiceLease>(redisClient, ServiceRegistryKey,
                                               data.nodeID.to_string(), TTS,
                                               HeartbeatInterval);
  }

  std::vector<NodeData> DiscoverServices() override
  {
    std::vector<NodeData> services;
    std::unordered_map<std::string, std::string> allServices;
    redisClient->hgetall(ServiceRegistryKey,
                         std::inserter(allServices, allServices.begin()));
    for (const auto& [key, value] : allServices)
    {
      try
      {
        NodeData nodeData;
        nodeData.from_json(nlohmann::json::parse(value));
        services.push_back(nodeData);
      }
      catch (const std::exception& e)
      {
        logger->error("Failed to parse NodeData from Redis: {}", e.what());
      }
    }
    return services;
  }

private:
  std::shared_ptr<sw::redis::Redis> redisClient;
  constexpr static std::string ServiceRegistryKey = "ServiceRegistry";
  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("RedisServiceDiscovery");
  constexpr static std::chrono::milliseconds TTS = std::chrono::milliseconds(
                                                 2000),
                                             HeartbeatInterval =
                                                 std::chrono::milliseconds(
                                                     1000);
};
} // namespace AtlasNet::Service