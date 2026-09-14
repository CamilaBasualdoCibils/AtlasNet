#include "AtlasNet/Core/Service/AtlasNetService.hpp"
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/Cluster/Transport/ClusterTransport.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sys/signalfd.h>

AtlasNet::AtlasNetService::AtlasNetService(AtlasNetServiceType service_type,
                                           int argc, char** argv)
    : argc(argc), argv(argv), signalFd(SetupSignals()),
      nodeID(AtlasNet::AtlasNetNodeID::Generate()), service_type(service_type),
      logger(spdlog::stdout_color_mt(
          "AtlasNet:" + std::string(boost::describe::enum_to_string(
                            service_type, "<INVALID>"))))
{
  #ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
  #endif 
  auto UnexpectedHandler = [](int signal)
  {
    std::cerr << "Unexpected signal: " << signal << std::endl;
    std::cerr << boost::stacktrace::stacktrace() << std::endl;
  };
  std::signal(SIGSEGV, UnexpectedHandler);
  std::signal(SIGABRT, UnexpectedHandler);
}
void AtlasNet::AtlasNetService::Run()
{
  namespace po = boost::program_options;
  po::options_description desc{"AtlasNetService Options"};
  AddOptions(desc);
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help"))
  {
    std::cerr << desc << std::endl;
    exit(0);
  }

  ParseOptions(vm);
  logger->info("Starting {}[{}] - {}...",
               boost::describe::enum_to_string(service_type, "<INVALID>"),
               nodeID.to_short_string(), nodeID.to_string());
  if (options.networkTransportType ==
      Network::Cluster::ClusterTransportType::INVALID)
  {
    logger->warn(
        "Use --network-transport or "
        "ATLASNET_NETWORK_TRANSPORT environment variable. defaulting to UDP.");
    options.networkTransportType = Network::Cluster::ClusterTransportType::UDP;
  }


  switch (options.networkTransportType)
  {

  case Network::Cluster::ClusterTransportType::INVALID:
    throw std::runtime_error(
        "Invalid cluster transport type. Use --cluster-transport or "
        "ATLASNET_NETWORK_TRANSPORT environment variable.");
  case Network::Cluster::ClusterTransportType::UDP:
    baseTransport = std::make_shared<Network::UDPNetworkTransport>(
        "ChannelBusNetworkTransport",
        Network::SocketAddress(Network::IPv6::Any(),
                               options.clusterListenPort));
    break;
  case Network::Cluster::ClusterTransportType::DPDK:
    throw std::runtime_error("DPDK cluster transport is not yet implemented.");
    break;
  }
  logger->info("Channel Bus listening on {}",
               baseTransport->GetListenPort());
  /* clusterTransport = std::make_shared<Network::Cluster::ClusterTransport>(
      baseTransport, nullptr); */

  HandshakeTransport = std::make_shared<Network::UDPNetworkTransport>(
      "HandshakeTransport",
      Network::SocketAddress(
          Network::IPv6::Any(),
          options.handshakeListenPort)); // Use ephemeral port for handshake
                                         // transport
  HandshakeRPC = std::make_shared<Network::RPC::NetworkTransportRPC>(
      "HandshakeRPC", Network::RPC::NetworkTransportRPC::Config{
                          .networkTransport = HandshakeTransport});
  logger->info("Handshake port: {}",
               HandshakeTransport->GetListenPort());
  InitializeChannels();
  // Initialize the service
  Initialize();

  // Start the main loop
  MainLoop();
}

void AtlasNet::AtlasNetService::AddOptions(
    boost::program_options::options_description& desc)
{
  namespace po = boost::program_options;

  desc.add_options()

      // Node Sockets
      ("cluster-port", po::value<uint16_t>(),
       "Internal cluster port for node-to-node communication. EX: 1925, "
       "Optional")
      // Handshake port
      ("handshake-port", po::value<uint16_t>(),
       "Handshake port for node-to-node communication. EX: 1926");
}
void AtlasNet::AtlasNetService::ParseOptions(
    const boost::program_options::variables_map& vm)
{

  options.clusterListenPort =
      vm.count("cluster-port")
          ? static_cast<uint16_t>(vm["cluster-port"].as<uint16_t>())
          : (std::getenv("ATLASNET_CLUSTER_PORT")
                 ? static_cast<uint16_t>(
                       std::stoi(std::getenv("ATLASNET_CLUSTER_PORT")))
                 : Network::PORT_EPHEMERAL); // Default to port 0 (ephemeral)
  options.handshakeListenPort =
      vm.count("handshake-port")
          ? static_cast<uint16_t>(vm["handshake-port"].as<uint16_t>())
          : (std::getenv("ATLASNET_HANDSHAKE_PORT")
                 ? static_cast<uint16_t>(
                       std::stoi(std::getenv("ATLASNET_HANDSHAKE_PORT")))
                 : Network::PORT_EPHEMERAL); // Default to port 0 (ephemeral)
  if (options.handshakeListenPort == Network::PORT_INVALID)
  {
    logger->error("Handshake port must be specified via --handshake-port or "
                  "ATLASNET_HANDSHAKE_PORT environment variable.");
    throw std::invalid_argument("Handshake port must be specified");
  }
}

void AtlasNet::AtlasNetService::MainLoop()
{
  while (stop_requested.load() == false)
  {
    std::optional<int> signal = CheckForSignal();
    if (signal.has_value())
    {
      switch (signal.value())
      {
      case SIGINT:
        logger->info("SIGINT: Ctrl+C");
        break;

      case SIGTERM:
        logger->info("SIGTERM");
        break;

      case SIGHUP:
        logger->info("SIGHUP: Reload config");
        break;

      case SIGQUIT:
        logger->info("SIGQUIT");
        break;
      default:
        logger->info("Received signal: {}. Ignoring.", signal.value());
        break;
      }
      stop_requested.store(true);
      continue;
    }
    Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
void AtlasNet::AtlasNetService::InitializeChannels()
{
  channelBus = std::make_shared<Network::Cluster::ChannelBus>(
      Network::Cluster::ChannelBus::ChannelBusOptions{.transport =
                                                          clusterTransport});
  GetLogger()->info("Channel bus initialized");
}
std::string AtlasNet::AtlasNetService::GetHostID()
{
  return "INVALID";
}
int AtlasNet::AtlasNetService::SetupSignals()
{
  sigset_t mask;
  sigemptyset(&mask);

  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGQUIT);
  if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0)
    throw std::runtime_error("pthread_sigmask failed");

  int signalFd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);

  if (signalFd == -1)
    throw std::runtime_error("signalfd failed");
  return signalFd;
}
std::optional<int> AtlasNet::AtlasNetService::CheckForSignal()
{
  signalfd_siginfo info;

  ssize_t n = read(signalFd, &info, sizeof(info));

  if (n != sizeof(info))
    return std::nullopt;
  return info.ssi_signo;
}
