
#include "AtlasNetNode.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Channel/ChannelBus.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPCCommons.hpp"
#include "AtlasNet/DB/Handshake.hpp"
#include <boost/describe/enum_to_string.hpp>
#include <cstdlib>
#include <future>

inline std::vector<AtlasNet::AtlasNetNode::NodeConfig::IngressSocketOption>
ParseSocketOptions(const std::vector<std::string>& ingressOptions)
{
  using AN = AtlasNet::AtlasNetService;
  std::vector<AtlasNet::AtlasNetNode::NodeConfig::IngressSocketOption> result;

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

    AtlasNet::AtlasNetNode::NodeConfig::IngressSocketOption option{};

    bool parsedType =
        boost::describe::enum_from_string(typeString, option.type);

    if (!parsedType)
    {
      throw std::runtime_error("Unknown ingress transport type: " + typeString);
    }

    option.port = 0;

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
        option.port = static_cast<uint16_t>(std::stoi(value));
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
        option.ExtraArgs += ",";

      option.ExtraArgs += extraArgs[i];
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
void AtlasNet::AtlasNetNode::AddOptions(
    boost::program_options::options_description& desc)
{
  AtlasNetService::AddOptions(desc);
  desc.add_options()
      // Ingress Sockets
      ("ingress-sockets",
       boost::program_options::value<std::vector<std::string>>()->multitoken(),
       "Ingress Socket Types. EX: SteamNetSock:port=8888;UDP:port=1262")
      // DB Host
      ("DB-host", boost::program_options::value<std::string>(),
       "Database host address. EX: 127.0.0.1")
      // DB Port
      ("DB-port", boost::program_options::value<uint16_t>(),
       "Database port. EX: 6379");
}
void AtlasNet::AtlasNetNode::Initialize()
{

  for (const auto& socket : nodeConfig.ingressSockets)
  {
    GetLogger()->info("Ingress Socket: {}:{} {}",
                      boost::describe::enum_to_string(socket.type, "<INVALID>"),
                      socket.port, socket.ExtraArgs);
  }
  if (!nodeConfig.dbHandshakeAddress.IsValid())
  {
    GetLogger()->error(
        "Database address is not valid. Please specify a valid DB host and "
        "port.");
    throw std::runtime_error("Database address is not valid.");
  }

  {
    //we want this jthread since we only want to poll handshake rpc on nodes during handshake, it is useless afterwards
    std::jthread handshakeRPCPollThread(
        [this](std::stop_token st)
        {
          while (!st.stop_requested())
          {
            GetHandshakeRPC().Poll(Network::PollType::NonBlocking);
            std::this_thread::yield();
          }
        });
    const int pingMaxAttempts = 10;
    for (int i = 0; i < pingMaxAttempts; i++)
    {
      GetLogger()->info("Pinging DB at {}... attempt {}/{}",
                        nodeConfig.dbHandshakeAddress.to_string(), i + 1,
                        pingMaxAttempts);

      auto pingResult = GetHandshakeRPC().Call<DB::RPC_DB_Ping>(
          nodeConfig.dbHandshakeAddress, 0);
      std::future_status status = pingResult.wait_for(std::chrono::seconds(1));
      bool Successful = status == std::future_status::ready;
      if (Successful)
      {
        Network::RPC::TRPCResult<int> result = pingResult.get();
        if (result.has_value())
        {
          GetLogger()->info("Successfully pinged DB at {}",
                            nodeConfig.dbHandshakeAddress.to_string());
          break;
        }
        else
        {
          Successful = false;
        }
      }

      if (!Successful)
      {

        if (i == pingMaxAttempts - 1)
        {
          GetLogger()->error(
              "Failed to ping DB at {} after {} attempts. Exiting.",
              nodeConfig.dbHandshakeAddress.to_string(), pingMaxAttempts);
          throw std::runtime_error(
              "Failed to ping DB after multiple attempts.");
        }
        GetLogger()->warn("Failed to ping DB at {}. Retrying...",
                          nodeConfig.dbHandshakeAddress.to_string());
      }
    }
    DB::RegisterNodeRequest registerRequest;
    registerRequest.handshakeAddress = nodeConfig.dbHandshakeAddress;
    //registerRequest.channelBusAddress = Network::SocketAddress(Network::IPv6::Any(),GetClusterTransport());
    registerRequest.nodeID = GetNodeID();
    std::future<Network::RPC::TRPCResult<DB::RegisterNodeResponse>> response =
        GetHandshakeRPC().Call<DB::RPC_DB_RegisterNode>(
            nodeConfig.dbHandshakeAddress, registerRequest);

    std::future_status registerStatus =
        response.wait_for(std::chrono::seconds(5));
    if (registerStatus != std::future_status::ready)
    {
      GetLogger()->error(
          "Failed to register node with DB at {}. Timeout after 5 seconds.",
          nodeConfig.dbHandshakeAddress.to_string());
      throw std::runtime_error("Failed to register node with DB.");
    }
    Network::RPC::TRPCResult<DB::RegisterNodeResponse> registerResult =
        response.get();
    if (!registerResult.has_value())
    {
      GetLogger()->error(
          "Failed to register node with DB at {}. Error: {}",
          nodeConfig.dbHandshakeAddress.to_string(),
          boost::describe::enum_to_string(registerResult.error(), "<INVALID>"));
      throw std::runtime_error("Failed to register node with DB.");
    }
    GetLogger()->info("Successfully registered node with DB at {}",
                      nodeConfig.dbHandshakeAddress.to_string());
  }
}

void AtlasNet::AtlasNetNode::ParseOptions(
    const boost::program_options::variables_map& vm)
{
  AtlasNetService::ParseOptions(vm);
  if (!(vm.count("DB-host") || std::getenv("ATLASNET_DB_HOST")) ||
      !(vm.count("DB-port") || std::getenv("ATLASNET_DB_PORT")))
  {
#if DEBUG
    nodeConfig.dbHandshakeAddress = Network::SocketAddress(
        Network::IPv4::Loopback(), ATLASNET_DB_DEBUG_HANDSHAKE_PORT);
    GetLogger()->warn(
        "Database host and port not specified. Using default values ({}) for "
        "development.",
        nodeConfig.dbHandshakeAddress.to_string());
#else
    GetLogger()->error(
        "Database host and port must be specified via command line DB-Host and "
        "DB-Port or "
        "environment variables ATLASNET_DB_HOST and ATLASNET_DB_PORT.");
    throw std::runtime_error("Database host and port must be specified");
#endif
  }
  else
  {
    Network::HostAddress dbHostAddress(vm.count("DB-host") &&
                                               !vm["DB-host"].empty()
                                           ? vm["DB-host"].as<std::string>()
                                           : std::getenv("ATLASNET_DB_HOST"));
    Network::PortType dbPort =
        vm.count("DB-port") && !vm["DB-port"].empty()
            ? static_cast<uint16_t>(vm["DB-port"].as<uint16_t>())
            : (std::getenv("ATLASNET_DB_PORT")
                   ? static_cast<uint16_t>(
                         std::stoi(std::getenv("ATLASNET_DB_PORT")))
                   : 0);

    nodeConfig.dbHandshakeAddress =
        Network::SocketAddress(dbHostAddress, dbPort);
    GetLogger()->info("Database address set to: {}",
                      nodeConfig.dbHandshakeAddress.to_string());
  }

  nodeConfig.ingressSockets = ParseSocketOptions(
      !vm["ingress-sockets"].empty()
          ? vm["ingress-sockets"].as<std::vector<std::string>>()
          : (std::getenv("ATLASNET_INGRESS_SOCKETS")
                 ? std::vector<std::string>{std::getenv(
                       "ATLASNET_INGRESS_SOCKETS")}
                 : std::vector<std::string>{}));
  if (nodeConfig.ingressSockets.empty())
  {
    GetLogger()->warn(
        "AtlasNetService: No ingress sockets specified.\n Use "
        "--ingress-sockets "
        "or ATLASNET_INGRESS_SOCKETS environment variable.\n This "
        "service will not "
        "accept any incoming client connections.");
  }
}