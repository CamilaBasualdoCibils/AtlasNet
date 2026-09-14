#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPC.hpp"
#include "AtlasNet/Core/Network/RPC/RPCCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include <cstddef>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shared_mutex>
#include <string>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class MockRPC : public AtlasNet::Network::RPC::RPC<AtlasNet::AtlasNetNodeID,
                                                   AtlasNet::AtlasNetNodeID>
{
  struct Packet
  {
    AtlasNet::AtlasNetNodeID source;
    AtlasNet::AtlasNetNodeID target;
    std::vector<std::byte> data;
  };
  static inline std::unordered_map<AtlasNet::AtlasNetNodeID,
                                   std::vector<Packet>>
      sentPackets;
  static inline std::shared_mutex sentPacketsMutex;
  AtlasNet::AtlasNetNodeID nodeID;

public:
  MockRPC(const std::string_view name, const AtlasNet::AtlasNetNodeID& id)
      : RPC(name), nodeID(id)
  {
  }
  AtlasNet::AtlasNetNodeID GetNodeID() const
  {
    return nodeID;
  }

protected:
  std::string CallerToString(const AtlasNet::UUID& caller) const override
  {
    return caller.to_string();
  }

  std::string
  TargetToString(const AtlasNet::AtlasNetNodeID& target) const override
  {
    return target.to_string();
  }

  void _ImplSendPacket(const AtlasNet::AtlasNetNodeID& target,
                       std::span<const std::byte> packetdata) override
  {
    std::unique_lock lock(sentPacketsMutex);
    sentPackets[target].emplace_back(
        Packet{.source = nodeID,
               .target = target,
               .data = {packetdata.begin(), packetdata.end()}});
  }

  void _ImplPoll(AtlasNet::Network::PollType pollType) override
  {
    std::unique_lock lock(sentPacketsMutex);
    std::vector<Packet> packets = std::move(sentPackets[nodeID]);
    lock.unlock();
    for (const auto& packet : packets)
    {
      HandleIncomingPacket(packet.source, packet.target, packet.data);
    }
  }
};

TEST(RPC, Call_Raw)
{
  AtlasNet::AtlasNetNodeID nodeID1 = AtlasNet::AtlasNetNodeID::Generate();
  AtlasNet::AtlasNetNodeID nodeID2 = AtlasNet::AtlasNetNodeID::Generate();
  MockRPC rpc1("Node1", nodeID1);
  MockRPC rpc2("Node2", nodeID2);
  std::atomic_bool callCompleted{false};
  std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}};
  auto boundHandler =
      [&](const MockRPC::CallContext&,
          std::span<const std::byte> data_r) -> std::vector<std::byte>
  {
    callCompleted = true;
    EXPECT_EQ(data_r.size(), data.size());
    for (size_t i = 0; i < data_r.size(); ++i)
    {
      EXPECT_EQ(data_r[i], data[i]);
    }
    return {};
  };
  rpc1.Bind("MockRPC.MockFunction", boundHandler);
  rpc2.Call(rpc1.GetNodeID(), "MockRPC.MockFunction", data);

  rpc1.Poll(AtlasNet::Network::PollType::NonBlocking);
  EXPECT_TRUE(callCompleted);
  // Add your assertions here
}
TEST(RPC, CallResponse_Raw)
{
  AtlasNet::AtlasNetNodeID nodeID1 = AtlasNet::AtlasNetNodeID::Generate();
  AtlasNet::AtlasNetNodeID nodeID2 = AtlasNet::AtlasNetNodeID::Generate();
  MockRPC rpc1("Node1", nodeID1);
  MockRPC rpc2("Node2", nodeID2);
  std::atomic_bool callCompleted{false};
  std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02}};
  auto boundHandler =
      [&](const MockRPC::CallContext&,
          std::span<const std::byte> data_r) -> std::vector<std::byte>
  {
    callCompleted = true;
    EXPECT_EQ(data_r.size(), data.size());
    for (size_t i = 0; i < data_r.size(); ++i)
    {
      EXPECT_EQ(data_r[i], data[i]);
    }
    return {std::byte{0x03}, std::byte{0x04}};
  };
  rpc1.Bind("MockRPC.MockFunction", boundHandler);
  auto future = rpc2.Call_R(rpc1.GetNodeID(), "MockRPC.MockFunction", data);

  rpc1.Poll(AtlasNet::Network::PollType::NonBlocking);
  rpc2.Poll(AtlasNet::Network::PollType::NonBlocking);
  EXPECT_TRUE(callCompleted);
  auto result = future.get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 2);
  EXPECT_EQ(result.value()[0], std::byte{0x03});
  EXPECT_EQ(result.value()[1], std::byte{0x04});
}
using SimpleCallMethod =
    AtlasNet::Network::RPC::RPCMethod<"MockRPC.SimpleCallMethod", void,
                                      std::string>;
TEST(RPC, Call)
{
  AtlasNet::AtlasNetNodeID nodeID1 = AtlasNet::AtlasNetNodeID::Generate();
  AtlasNet::AtlasNetNodeID nodeID2 = AtlasNet::AtlasNetNodeID::Generate();
  MockRPC rpc1("Node1", nodeID1);
  MockRPC rpc2("Node2", nodeID2);
  std::atomic_bool callCompleted{false};
  std::string data = "Hello, world!";
  auto boundHandler = [&](const MockRPC::CallContext& ctx,
                          const std::string& data_r) -> void
  {
    callCompleted = true;
    EXPECT_EQ(data_r, data);
  };
  rpc1.Bind<SimpleCallMethod>(boundHandler);
  rpc2.Call<SimpleCallMethod>(rpc1.GetNodeID(), data);

  rpc1.Poll( AtlasNet::Network::PollType::NonBlocking);
  EXPECT_TRUE(callCompleted);
}
using SimpleCallResponseMethod =
    AtlasNet::Network::RPC::RPCMethod<"MockRPC.SimpleCallResponseMethod",
                                      std::string, std::string>;
TEST(RPC, CallResponse)
{
  AtlasNet::AtlasNetNodeID nodeID1 = AtlasNet::AtlasNetNodeID::Generate();
  AtlasNet::AtlasNetNodeID nodeID2 = AtlasNet::AtlasNetNodeID::Generate();
  MockRPC rpc1("Node1", nodeID1);
  MockRPC rpc2("Node2", nodeID2);
  std::atomic_bool callCompleted{false};
  std::string data = "Hello", data2 = " , world!";
  auto boundHandler = [&callCompleted, data2](const MockRPC::CallContext& ctx,
                              const std::string& data_r) -> std::string
  {
    callCompleted = true;
    return data_r + data2;
  };
  rpc1.Bind<SimpleCallResponseMethod>(boundHandler);
  auto future = rpc2.Call<SimpleCallResponseMethod>(rpc1.GetNodeID(), data);

  rpc1.Poll(AtlasNet::Network::PollType::NonBlocking);
  rpc2.Poll(AtlasNet::Network::PollType::NonBlocking);
  EXPECT_TRUE(callCompleted);
  auto result = future.get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "Hello , world!");
}