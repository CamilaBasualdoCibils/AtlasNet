#include "Configuration.hpp"
#include <boost/program_options.hpp>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace AtlasNet::Standalone
{
uint16_t ParsePort(const std::string& value)
{
  unsigned port = 0;
  auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), port);
  if (error != std::errc{} || end != value.data() + value.size() ||
      port > 65535)
    throw std::invalid_argument("Invalid port: " + value);
  return static_cast<uint16_t>(port);
}

NodeCapability ParseCapabilities(const std::string& value)
{
  if (value == "None")
    return NodeCapability::None;
  NodeCapability result = NodeCapability::None;
  std::stringstream input(value);
  std::string item;
  if (value.empty() || value.back() == ',')
    throw std::invalid_argument("Empty capability");
  while (std::getline(input, item, ','))
  {
    if (item == "Shard")
      result = result | NodeCapability::Shard;
    else if (item == "ClientIngress")
      result = result | NodeCapability::ClientIngress;
    else if (item == "ControllerEligible")
      result = result | NodeCapability::ControllerEligible;
    else if (item == "Database")
      result = result | NodeCapability::Database;
    else
      throw std::invalid_argument("Unknown capability: " + item);
  }
  return result;
}

inline std::vector<AtlasNet::NodeConfig::ClientIngressListener>
ParseSocketOptions(const std::vector<std::string>& ingressOptions)
{
  std::vector<AtlasNet::NodeConfig::ClientIngressListener> result;

  auto parseEntry = [&](const std::string& entry)
  {
    if (entry.empty())
      return;

    auto colon = entry.find(':');

    std::string typeString;
    std::string args;

    if (colon == std::string::npos)
    {
      typeString = entry;
    }
    else
    {
      typeString = entry.substr(0, colon);
      args = entry.substr(colon + 1);
    }

    if (typeString.empty())
      throw std::invalid_argument("Ingress transport name cannot be empty");
    AtlasNet::NodeConfig::ClientIngressListener option{};
    option.transport = std::move(typeString);

    // Accept the concise stable identifier syntax (TCP:5919) as well as the
    // existing key/value form (TCP:port=5919).
    if (!args.empty() && args.find('=') == std::string::npos &&
        args.find(',') == std::string::npos)
    {
      option.config.port = ParsePort(args);
      result.push_back(std::move(option));
      return;
    }

    // Parse comma-separated args
    std::stringstream stream(args);
    std::string arg;

    std::vector<std::string> extraArgs;

    while (std::getline(stream, arg, ','))
    {
      auto equals = arg.find('=');

      if (equals == std::string::npos)
      {
        extraArgs.push_back(arg);
        continue;
      }

      auto key = arg.substr(0, equals);
      auto value = arg.substr(equals + 1);

      if (key == "port")
      {
        option.config.port = ParsePort(value);
      }
      else
      {
        extraArgs.push_back(arg);
      }
    }

    // Preserve everything transport-specific
    for (size_t i = 0; i < extraArgs.size(); i++)
    {
      if (i)
        option.config.arguments += ",";

      option.config.arguments += extraArgs[i];
    }

    result.push_back(std::move(option));
  };

  for (const auto& input : ingressOptions)
  {
    // Support either:
    // ["a", "b"]
    // or:
    // ["a;b"]
    std::stringstream stream(input);
    std::string entry;

    while (std::getline(stream, entry, ';'))
    {
      parseEntry(entry);
    }
  }

  return result;
}

Configuration ParseConfiguration(int argc, const char* const* argv)
{
  namespace po = boost::program_options;
  po::options_description description("AtlasNet node options");
  description.add_options()("help", "Show options")(
      "cluster-port", po::value<std::string>(), "Cluster UDP port")(
      "handshake-port", po::value<std::string>(), "Registry RPC UDP port")(
      "DB-host", po::value<std::string>(), "Upstream AtlasNet registry host")(
      "DB-port", po::value<std::string>(),
      "Upstream AtlasNet registry RPC port")("network-transport",
                                             po::value<std::string>(),
                                             "UDP (DPDK is not implemented)")(
      "capabilities", po::value<std::string>(),
      "Comma-separated Shard,ClientIngress,ControllerEligible,Database or "
      "None")("valkey-uri", po::value<std::string>(),
              "Remote RESP URI required for Database capability")(
      "module", po::value<std::vector<std::string>>()->multitoken(),
      "AtlasNet node module shared object (repeatable)")(
      "ingress-sockets", po::value<std::vector<std::string>>()->multitoken(),
      "Named client ingress listeners (for example TCP:port=5919)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, description), vm);
  po::notify(vm);
  Configuration result;
  if (vm.count("help"))
  {
    std::cout << description << '\n';
    result.help = true;
    return result;
  }
  auto setting = [&](const char* option,
                     const char* environment) -> std::optional<std::string>
  {
    if (vm.count(option))
      return vm[option].as<std::string>();
    if (const char* value = std::getenv(environment))
      return value;
    return std::nullopt;
  };
  auto& config = result.node;
  if (vm.count("module"))
    config.modules = vm["module"].as<std::vector<std::string>>();
  else if (const char* value = std::getenv("ATLASNET_MODULES"))
  {
    std::stringstream modules(value);
    std::string path;
    while (std::getline(modules, path, ';'))
      if (!path.empty())
        config.modules.push_back(std::move(path));
  }
  if (auto value = setting("cluster-port", "ATLASNET_CLUSTER_PORT"))
    config.transport.clusterListenPort = ParsePort(*value);
  if (auto value = setting("handshake-port", "ATLASNET_HANDSHAKE_PORT"))
    config.transport.handshakeListenPort = ParsePort(*value);
  if (auto value = setting("network-transport", "ATLASNET_NETWORK_TRANSPORT"))
    if (*value != "UDP")
      throw std::invalid_argument("Only UDP transport is currently supported");
  if (auto value = setting("capabilities", "ATLASNET_CAPABILITIES"))
    config.capabilities = ParseCapabilities(*value);
  result.valkeyURI = setting("valkey-uri", "ATLASNET_VALKEY_URI").value_or("");
  if (HasCapability(config.capabilities, NodeCapability::Database) &&
      result.valkeyURI.empty())
    throw std::invalid_argument(
        "Database capability requires --valkey-uri or ATLASNET_VALKEY_URI");
  auto host = setting("DB-host", "ATLASNET_DB_HOST");
  auto port = setting("DB-port", "ATLASNET_DB_PORT");
  if (host.has_value() != port.has_value())
    throw std::invalid_argument("Specify both DB-host and DB-port");
  if (host)
  {
    const auto parsedPort = ParsePort(*port);
    if (parsedPort == 0)
      throw std::invalid_argument("DB-port cannot be zero");
    config.dbHandshakeAddress =
        Network::SocketAddress(Network::HostAddress(*host), parsedPort);
  }
  else if (!HasCapability(config.capabilities, NodeCapability::Database))
  {
#ifdef DEBUG
    config.dbHandshakeAddress = Network::SocketAddress(
        Network::IPv4::Loopback(), ATLASNET_DB_DEBUG_HANDSHAKE_PORT);
#else
    throw std::invalid_argument(
        "Specify DB-host and DB-port for the upstream registry");
#endif
  }
  std::vector<std::string> ingress;
  if (vm.count("ingress-sockets"))
    ingress = vm["ingress-sockets"].as<std::vector<std::string>>();
  else if (auto value = std::getenv("ATLASNET_INGRESS_SOCKETS"))
    ingress.emplace_back(value);
  config.clientIngressListeners = ParseSocketOptions(ingress);
  if (!config.clientIngressListeners.empty() &&
      !HasCapability(config.capabilities, NodeCapability::ClientIngress))
    throw std::invalid_argument(
        "Ingress sockets require ClientIngress capability");
  return result;
}
} // namespace AtlasNet::Standalone
