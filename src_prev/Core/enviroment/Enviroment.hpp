/**
 * @page env_configuration AtlasNet Environment Configuration
 *
 * @tableofcontents
 *
 * # Overview
 *
 * The `AtlasNet::Env` class provides centralized access to runtime
 * configuration through environment variables.
 *
 * These values are resolved once during static initialization and are
 * intended to configure networking, orchestration, database access,
 * shard startup behavior, and runtime debugging.
 *
 * ---
 *
 * # Configuration Source
 *
 * Every configuration entry follows this pattern:
 *
 * @code{.cpp}
 * GetEnvVarOrDefault("ENV_NAME", "default_value")
 * @endcode
 *
 * If the environment variable does not exist, the default value is used.
 *
 * ---
 *
 * # Runtime Architecture
 *
 * @dot
 * digraph AtlasNetEnv {
 *     rankdir=LR;
 *
 *     Environment [shape=box];
 *     EnvClass [shape=box];
 *     Controller [shape=box];
 *     Agent [shape=box];
 *     Shard [shape=box];
 *     Database [shape=box];
 *
 *     Environment -> EnvClass;
 *
 *     EnvClass -> Controller;
 *     EnvClass -> Agent;
 *     EnvClass -> Shard;
 *     EnvClass -> Database;
 * }
 * @enddot
 *
 * ---
 *
 * # Environment Variables
 *
 * ## Stack Configuration
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_STACK_NAME` | `DefaultStackName` | Logical deployment stack name
 * | | `ATLASNET_TICK_RATE` | `20` | Server tick/update rate |
 *
 * ---
 *
 * ## RPC Configuration
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_RPC_PORT` | `41001` | RPC communication port |
 *
 * ---
 *
 * ## Internal Messaging
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_INTERNAL_MESSAGE_PORT` | `41000` | Internal inter-service
 * messaging port |
 *
 * ---
 *
 * ## Database Configuration
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_DATABASE_HOST_NAME` | `Atlasnet-Database` | Database hostname |
 * | `ATLASNET_DATABASE_NAMESPACE` | `atlasnet:` | Database key namespace prefix
 * | | `ATLASNET_DATABASE_PORT` | `6379` | Database port |
 *
 * ---
 *
 * ## Shard Configuration
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_SHARD_CPU_RESERVE` | `4` | Reserved CPU count for shard workers |
 * | `ATLASNET_DOCKER_SHARD_IMAGE_NAME` | `atlasnet/shard:latest` | Docker image
 * used for shard containers |
 *
 * ---
 *
 * ## Startup World Definition
 *
 * The `ATLASNET_STARTUP_WORLDS` variable contains a YAML definition
 * describing worlds automatically created during startup.
 *
 * Default:
 *
 * @code{.yaml}
 * worlds:
 *   - name: "Main"
 *     SpaceType: "Cartesian2D"
 * @endcode
 *
 * ---
 *
 * # Debugging
 *
 * | Variable | Default | Description |
 * |----------|----------|-------------|
 * | `ATLASNET_DEBUG_MODE` | `1` | Enables verbose debug behavior |
 *
 * ---
 *
 * # Example Docker Compose Usage
 *
 * @code{.yaml}
 * services:
 *   shard:
 *     image: atlasnet/shard:latest
 *     environment:
 *       ATLASNET_STACK_NAME: "Production"
 *       ATLASNET_RPC_PORT: "41001"
 *       ATLASNET_DATABASE_HOST_NAME: "redis"
 *       ATLASNET_DEBUG_MODE: "0"
 * @endcode
 *
 * ---
 *
 * # Notes
 *
 * - All values are statically initialized.
 * - Integer parsing uses `std::atoi`.
 * - Boolean values are interpreted as:
 *
 * @code{.txt}
 * 0 = false
 * non-zero = true
 * @endcode
 *
 * - Environment variables should be configured before process startup.
 *
 * ---
 *
 * # Related Systems
 *
 * - RPC networking
 * - Database connectivity
 * - Shard orchestration
 * - Container deployment
 * - World bootstrap initialization
 */
#pragma once

#include "atlasnet/controller/ServiceAdapterEnums.hpp"
#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "boost/describe/enum_from_string.hpp"
#include <cstdint>
#include <cstdlib>
namespace AtlasNet
{

class Env
{

  static inline const char* GetEnvVarOrDefault(const char* varName,
                                               const char* defaultValue)
  {
    const char* value = std::getenv(varName);

    if (!value || value[0] == '\0')
      return defaultValue;
    std::string valueStr(value);
    if (valueStr.empty())
      return defaultValue;

    return value;
  }

public:
  const static inline std::string StackName =
      GetEnvVarOrDefault("ATLASNET_STACK_NAME", "DefaultStackName");
  const static inline uint32_t TickRate =
      std::atoi(GetEnvVarOrDefault("ATLASNET_TICK_RATE", "20"));

  // const static inline PortType RPCPort = static_cast<PortType>(
  //     std::atoi(GetEnvVarOrDefault("ATLASNET_RPC_PORT", "41001")));

  const static inline std::string DatabaseHostName =
      GetEnvVarOrDefault("ATLASNET_DATABASE_HOST_NAME", "atlasnet-database");

  const static inline std::string DatabaseNamespace =
      GetEnvVarOrDefault("ATLASNET_DATABASE_NAMESPACE", "atlasnet:");
  const static inline Network::PortType DatabasePort = static_cast<Network::PortType>(
      std::atoi(GetEnvVarOrDefault("ATLASNET_DATABASE_TCP_PORT", "6379")));
  const static inline std::string InternalMessagingSubnet =
      GetEnvVarOrDefault("ATLASNET_NETWORK_SUBNET", "10.0.0.0/16");
  const static inline Network::PortType InternalMessagePort = static_cast<Network::PortType>(
      std::atoi(GetEnvVarOrDefault("ATLASNET_INTERNAL_MESSAGE_PORT", "41000")));
  const static inline bool DebugMode =
      std::atoi(GetEnvVarOrDefault("ATLASNET_DEBUG_MODE", "1")) != 0;

  const static inline std::string StartupWorlds = GetEnvVarOrDefault(
      "ATLASNET_STARTUP_WORLDS",
      R"({"worlds":{"MainWorld":{"SpaceType":"Cartesian2D","Heuristic":"KDTree"}}})");

  const static inline uint32_t ShardCPUReserve = static_cast<uint32_t>(
      std::atoi(GetEnvVarOrDefault("ATLASNET_SHARD_CPU_RESERVE", "4")));
  const static inline ServiceAdapterType ControllerServiceBackend = []()
  {
    ServiceAdapterType type = ServiceAdapterType::INVALID;
    const char* envValue = std::getenv("ATLASNET_CONTROLLER_SERVICE_BACKEND");
    std::string UpperEnvValue = envValue ? std::string(envValue) : "";
    std::transform(UpperEnvValue.begin(), UpperEnvValue.end(), UpperEnvValue.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    envValue = UpperEnvValue.c_str();
    if (envValue && envValue[0] != '\0')
    {
      bool parse = boost::describe::enum_from_string(envValue, type);
      if (!parse)
        type = ServiceAdapterType::INVALID;
    }
    return type;
  }();

  const static inline std::string ShardDefaultImage = GetEnvVarOrDefault(
      "ATLASNET_SHARD_DEFAULT_IMAGE", "INVALID_SHARD_IMAGE_NAME");
  const static inline std::string Docker_NetworkName =
      GetEnvVarOrDefault("ATLASNET_DOCKER_NETWORK_NAME", "INVALID_NETWORK_NAME");

  const static inline std::string DockerSocketPath =
      GetEnvVarOrDefault("ATLASNET_DOCKER_SOCKET_PATH", "/var/run/docker.sock");

  const static inline Network::PortType GatewayListenPort = static_cast<Network::PortType>(
      std::atoi(GetEnvVarOrDefault("ATLASNET_PROXY_LISTEN_PORT", "42000")));
};

} // namespace AtlasNet