#include "AtlasNet/Core/Network/RPC/RPCMethod.hpp"
#include "AtlasNet/Node/Shard/ShardManager.hpp"
#include <gtest/gtest.h>
#include <thread>
using namespace AtlasNet;
namespace
{
using Echo = Network::RPC::RPCMethod<"Test.Echo", std::string, std::string>;
using Ready = Network::RPC::RPCMethod<"Test.Ready", void, ShardID>;
using Crash = Network::RPC::RPCMethod<"Test.Crash", void>;
ShardManager MakeManager()
{
  return ShardManager({.workerExecutable = ATLASNET_SHARD_WORKER,
                       .modules = {ATLASNET_TEST_SHARD_MODULE}});
}
template <typename Predicate>
bool PollUntil(ShardManager& manager, Predicate predicate)
{
  for (int i = 0; i < 500; ++i)
  {
    manager.Poll();
    if (predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}
}
TEST(ShardManager, AllocatesSequentialNodeLocalIDs)
{
  auto first = MakeManager();
  EXPECT_EQ(first.CreateShard().value, 0u);
  EXPECT_EQ(first.CreateShard().value, 1u);
  EXPECT_EQ(first.CreateShard().value, 2u);
  auto second = MakeManager();
  EXPECT_EQ(second.CreateShard().value, 0u);
}
TEST(ShardManager, BidirectionalRPCAndResponse)
{
  auto manager = MakeManager();
  bool ready = false;
  manager.RPC().Bind<Ready>(
      [&](const auto& context, ShardID id)
      {
        ready = true;
        EXPECT_EQ(context.caller.value, id.value);
      });
  const auto id = manager.CreateShard();
  ASSERT_TRUE(PollUntil(manager, [&] { return ready; }));
  auto response = manager.RPC().Call<Echo>(id, std::string("hello"));
  ASSERT_TRUE(PollUntil(manager, [&]
    { return response.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }));
  const auto result = response.get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "hello");
}
TEST(ShardManager, DetectsCrashAndDestroysWorkers)
{
  auto manager = MakeManager();
  bool ready = false;
  manager.RPC().Bind<Ready>(
      [&](const auto&, ShardID) { ready = true; });
  const auto crashed = manager.CreateShard();
  ASSERT_TRUE(PollUntil(manager, [&] { return ready; }));
  manager.RPC().Call<Crash>(crashed, {});
  EXPECT_TRUE(PollUntil(manager, [&] { return !manager.IsRunning(crashed); }));
  EXPECT_EQ(manager.Size(), 0u);
  const auto clean = manager.CreateShard();
  ASSERT_TRUE(manager.IsRunning(clean));
  manager.DestroyShard(clean);
  EXPECT_FALSE(manager.IsRunning(clean));
  EXPECT_EQ(manager.Size(), 0u);
}
TEST(ShardManager, ShutdownTerminatesAllWorkers)
{
  auto manager = MakeManager();
  manager.CreateShard();
  manager.CreateShard();
  manager.Shutdown();
  EXPECT_EQ(manager.Size(), 0u);
}
