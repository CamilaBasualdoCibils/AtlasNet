#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/DB/DebugMirror.hpp"
#include "AtlasNet/DB/Keys.hpp"
#include "AtlasNet/Node/RPC/Database.hpp"
#include "Standalone/Configuration.hpp"
#include <gtest/gtest.h>

using namespace AtlasNet;

TEST(NodeConfiguration, CapabilitiesAreIndependentFlags)
{
  const auto value =
      Standalone::ParseCapabilities("Database,Shard,ControllerEligible");
  EXPECT_TRUE(HasCapability(value, NodeCapability::Database));
  EXPECT_TRUE(HasCapability(value, NodeCapability::Shard));
  EXPECT_FALSE(HasCapability(value, NodeCapability::ClientIngress));
  EXPECT_EQ(Standalone::ParseCapabilities("None"), NodeCapability::None);
  EXPECT_THROW(Standalone::ParseCapabilities("ValkeyModule"),
               std::invalid_argument);
  EXPECT_THROW(Standalone::ParseCapabilities("Database,"),
               std::invalid_argument);
  EXPECT_THROW(Standalone::ParseCapabilities(""), std::invalid_argument);
}

TEST(NodeConfiguration, PortsRejectTruncationAndTrailingCharacters)
{
  EXPECT_EQ(Standalone::ParsePort("65535"), 65535);
  EXPECT_EQ(Standalone::ParsePort("0"), 0);
  for (auto value : {"-1", "65536", "123x", ""})
    EXPECT_THROW(Standalone::ParsePort(value), std::invalid_argument);
}

TEST(NodeConfiguration, StandaloneDatabaseUsesExplicitRemoteStorage)
{
  const char* argv[] = {
      "node",         "--capabilities",       "Database,Shard",
      "--valkey-uri", "tcp://127.0.0.1:6379", "--handshake-port",
      "12345"};
  auto config = Standalone::ParseConfiguration(7, argv);
  EXPECT_TRUE(
      HasCapability(config.node.capabilities, NodeCapability::Database));
  EXPECT_EQ(config.node.transport.handshakeListenPort, 12345);
  EXPECT_EQ(config.valkeyURI, "tcp://127.0.0.1:6379");
  EXPECT_FALSE(config.node.dbHandshakeAddress.IsValid());
}

TEST(NodeConfiguration, DatabaseCannotStartWithoutBackend)
{
  NodeConfig config;
  config.capabilities = NodeCapability::Database;
  EXPECT_THROW(AtlasNetNode node(config), std::invalid_argument);
}

TEST(NodeConfiguration, IngressRequiresCapability)
{
  NodeConfig config;
  config.capabilities = NodeCapability::None;
  config.ingressSockets.push_back({});
  EXPECT_THROW(AtlasNetNode node(config), std::invalid_argument);
}

TEST(DatabaseRPC, RegistrationCapabilitiesSurviveSerialization)
{
  RPC::Database::RegisterNodeRequest request;
  request.nodeID = AtlasNetNodeID::Generate();
  request.capabilities = NodeCapability::Database | NodeCapability::Shard;
  request.handshakeAddress = Network::SocketAddress("127.0.0.1:1234");
  request.channelBusAddress = Network::SocketAddress("127.0.0.1:1235");
  NetBinaryWriter writer;
  writer(request);
  const auto data = writer.Release();
  NetBinaryReader reader(data);
  RPC::Database::RegisterNodeRequest decoded;
  reader(decoded);
  EXPECT_EQ(decoded.nodeID, request.nodeID);
  EXPECT_EQ(decoded.capabilities, request.capabilities);
  EXPECT_EQ(decoded.handshakeAddress, request.handshakeAddress);
}

TEST(DatabaseDebugMirror, RegistrationIsReadableJSON)
{
  RPC::Database::RegisterNodeRequest request;
  request.nodeID = AtlasNetNodeID::Generate();
  request.handshakeAddress = Network::SocketAddress("127.0.0.1:1234");
  request.channelBusAddress = Network::SocketAddress("127.0.0.1:1235");
  request.capabilities = NodeCapability::Database | NodeCapability::Shard;

  const auto json = DB::DebugMirror::ToJSON(request);
  EXPECT_EQ(json.at("nodeID"), request.nodeID.to_string());
  EXPECT_EQ(json.at("handshakeAddress"), "127.0.0.1:1234");
  EXPECT_EQ(json.at("channelBusAddress"), "127.0.0.1:1235");
  EXPECT_EQ(json.at("capabilityMask"),
            static_cast<uint64_t>(request.capabilities));
  EXPECT_EQ(json.at("capabilities"), _Json({"Shard", "Database"}));
  EXPECT_EQ(DB::Keys::DebugMirror(DB::Keys::RegisteredNodes),
            "AtlasNet:RegisteredNodes_debug");
}

namespace
{
class RecordingBackend final : public DB::IDatabaseBackend
{
public:
  int writes = 0;
  RPC::Database::RegisterNodeRequest last;
  RPC::Database::RegisterNodeResponse
  RegisterNode(const RPC::Database::RegisterNodeRequest& request) override
  {
    ++writes;
    last = request;
    return RPC::Database::RegisterNodeResponse::SUCCESS;
  }
};
} // namespace

TEST(NodeRuntime, DatabaseUsesInjectedStorageWithoutUpstreamHandshake)
{
  NodeConfig config;
  config.capabilities = NodeCapability::Database | NodeCapability::Shard;
  auto backend = std::make_unique<RecordingBackend>();
  auto* recording = backend.get();
  AtlasNetNode node(config, std::move(backend));
  node.Start();
  EXPECT_EQ(recording->writes, 1);
  EXPECT_EQ(recording->last.capabilities, config.capabilities);
  node.Poll();
}

namespace
{
class ScopedEnvironment
{
public:
  ScopedEnvironment(const char* name, const char* value) : name(name)
  {
    if (auto previous = std::getenv(name))
      saved = previous;
    if (value)
      setenv(name, value, 1);
    else
      unsetenv(name);
  }
  ~ScopedEnvironment()
  {
    if (saved)
      setenv(name.c_str(), saved->c_str(), 1);
    else
      unsetenv(name.c_str());
  }

private:
  std::string name;
  std::optional<std::string> saved;
};
} // namespace

TEST(NodeConfiguration, EnvironmentCapabilitiesAndCommandLineOverride)
{
  ScopedEnvironment capabilities("ATLASNET_CAPABILITIES", "Database");
  ScopedEnvironment uri("ATLASNET_VALKEY_URI", "tcp://127.0.0.1:16379");
  ScopedEnvironment host("ATLASNET_DB_HOST", nullptr);
  ScopedEnvironment port("ATLASNET_DB_PORT", nullptr);
  const char* argv[] = {"node", "--capabilities", "Database,Shard"};
  const auto config = Standalone::ParseConfiguration(3, argv);
  EXPECT_TRUE(HasCapability(config.node.capabilities, NodeCapability::Shard));
  EXPECT_EQ(config.valkeyURI, "tcp://127.0.0.1:16379");
  const char* envOnly[] = {"node"};
  EXPECT_EQ(Standalone::ParseConfiguration(1, envOnly).node.capabilities,
            NodeCapability::Database);
}

TEST(NodeConfiguration, DatabaseRequiresExplicitStorageLocation)
{
  ScopedEnvironment uri("ATLASNET_VALKEY_URI", nullptr);
  const char* argv[] = {"node", "--capabilities", "Database"};
  EXPECT_THROW(Standalone::ParseConfiguration(3, argv), std::invalid_argument);
}
