// SpatialObject unit tests (EntityLedger replacement).
// IDs under test are AtlasNetEntityID / AtlasNetClientID (= Snowflake).
// Run: ctest -L SpatialObject --output-on-failure
//   or: ./AtlasNet_tests_SpatialObject --gtest_filter='SpatialObject*'

#include "AtlasNet/Core/CmdSig/Command.hpp"
#include "AtlasNet/Core/CmdSig/CommandDispatcher.hpp"
#include "AtlasNet/Core/CmdSig/Ordering.hpp"
#include "AtlasNet/Core/CmdSig/Signal.hpp"
#include "AtlasNet/Core/Network/Intent/ClusterIntentResolver.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/Core/SpatialObject/EnttComponentBackend.hpp"
#include "AtlasNet/Core/SpatialObject/LockFreeSpatialObjectIndex.hpp"
#include "AtlasNet/Core/SpatialObject/SpatialObjectStore.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace AtlasNet;
using namespace AtlasNet::SpatialObject;
using namespace AtlasNet::CmdSig;

namespace
{

AtlasNetEntityID MakeId(uint64_t v)
{
  return AtlasNetEntityID{v};
}

PublishedState MakeState(AtlasNetEntityID id, double x = 0)
{
  PublishedState s;
  s.id = id;
  s.x = x;
  s.roles.alive = true;
  s.roles.isActor = true;
  return s;
}

} // namespace

TEST(SpatialObjectIndex, InsertLookupRemove)
{
  LockFreeSpatialObjectIndex index(64);
  const auto id = MakeId(42);
  EXPECT_EQ(index.Insert(id, MakeState(id, 1.5)), InsertResult::Ok);
  EXPECT_TRUE(index.Contains(id));
  auto handle = index.Find(id);
  ASSERT_TRUE(handle);
  auto pub = index.ReadPublished(id);
  ASSERT_TRUE(pub);
  EXPECT_DOUBLE_EQ(pub->x, 1.5);
  EXPECT_EQ(index.Insert(id, MakeState(id)), InsertResult::AlreadyExists);
  EXPECT_EQ(index.Remove(id), RemoveResult::Ok);
  EXPECT_FALSE(index.Contains(id));
  EXPECT_EQ(index.Remove(*handle), RemoveResult::StaleHandle);
}

TEST(SpatialObjectIndex, TombstoneReuse)
{
  LockFreeSpatialObjectIndex index(32);
  const auto id = MakeId(7);
  ASSERT_EQ(index.Insert(id, MakeState(id)), InsertResult::Ok);
  auto h1 = index.Find(id);
  ASSERT_TRUE(h1);
  ASSERT_EQ(index.Remove(id), RemoveResult::Ok);
  ASSERT_EQ(index.Insert(id, MakeState(id, 9)), InsertResult::Ok);
  auto h2 = index.Find(id);
  ASSERT_TRUE(h2);
  EXPECT_NE(h1->generation, h2->generation);
  EXPECT_FALSE(index.ReadPublished(*h1).has_value());
  auto pub = index.ReadPublished(*h2);
  ASSERT_TRUE(pub);
  EXPECT_DOUBLE_EQ(pub->x, 9);
}

TEST(SpatialObjectIndex, ConcurrentAddRemove)
{
  LockFreeSpatialObjectIndex index(4096);
  constexpr int kThreads = 4;
  constexpr int kPerThread = 200;
  std::barrier sync(kThreads * 2);

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t)
  {
    threads.emplace_back(
        [&, t]()
        {
          sync.arrive_and_wait();
          for (int i = 0; i < kPerThread; ++i)
          {
            const auto id = MakeId(static_cast<uint64_t>(t * kPerThread + i + 1));
            (void)index.Insert(id, MakeState(id));
          }
        });
  }
  for (int t = 0; t < kThreads; ++t)
  {
    threads.emplace_back(
        [&, t]()
        {
          sync.arrive_and_wait();
          for (int i = 0; i < kPerThread; ++i)
          {
            const auto id = MakeId(static_cast<uint64_t>(t * kPerThread + i + 1));
            for (int spin = 0; spin < 1000; ++spin)
            {
              if (index.Contains(id))
              {
                (void)index.Remove(id);
                break;
              }
            }
          }
        });
  }
  for (auto& th : threads)
  {
    th.join();
  }
  // Remaining membership must be consistent with Contains/Find
  for (int t = 0; t < kThreads; ++t)
  {
    for (int i = 0; i < kPerThread; ++i)
    {
      const auto id = MakeId(static_cast<uint64_t>(t * kPerThread + i + 1));
      const bool c = index.Contains(id);
      EXPECT_EQ(c, index.Find(id).has_value());
      if (c)
      {
        EXPECT_TRUE(index.ReadPublished(id).has_value());
      }
    }
  }
}

TEST(SpatialObjectIndex, SeqlockNoTear)
{
  LockFreeSpatialObjectIndex index(64);
  const auto id = MakeId(99);
  ASSERT_EQ(index.Insert(id, MakeState(id, 0)), InsertResult::Ok);

  std::atomic<bool> stop{false};
  std::thread writer(
      [&]()
      {
        double v = 0;
        while (!stop.load(std::memory_order_relaxed))
        {
          PublishedState s = MakeState(id, v);
          s.y = v;
          s.z = v;
          ASSERT_TRUE(index.Publish(id, s));
          v += 1.0;
        }
      });

  for (int i = 0; i < 5000; ++i)
  {
    auto pub = index.ReadPublished(id);
    ASSERT_TRUE(pub);
    EXPECT_DOUBLE_EQ(pub->x, pub->y);
    EXPECT_DOUBLE_EQ(pub->y, pub->z);
  }
  stop.store(true, std::memory_order_relaxed);
  writer.join();
}

TEST(SpatialObjectStore, SpawnDespawnActorAndClient)
{
  LockFreeSpatialObjectIndex index(128);
  EnttComponentBackend backend;
  const AtlasNetNodeID node = UUID::Generate();
  SpatialObjectStore store(index, backend, node);

  SpawnActorRequest actor{.id = MakeId(1),
                          .info = ObjectInfo{.worldId = node, .typeTag = 1},
                          .transform = Transform{.x = 3, .y = 4, .z = 5}};
  ASSERT_TRUE(store.SpawnActor(actor));
  EXPECT_TRUE(store.IsActor(actor.id));
  EXPECT_FALSE(store.IsClient(actor.id));
  auto pub = store.ReadPublished(actor.id);
  ASSERT_TRUE(pub);
  EXPECT_DOUBLE_EQ(pub->x, 3);

  SpawnClientActorRequest client{
      .id = MakeId(2),
      .info = ObjectInfo{.worldId = node},
      .transform = {},
      .clientId = MakeId(100)};
  ASSERT_TRUE(store.SpawnClientActor(client));
  EXPECT_TRUE(store.IsClient(client.id));
  EXPECT_TRUE(store.IsActor(client.id));

  ASSERT_TRUE(store.UpdateTransform(actor.id, Transform{.x = 10}));
  pub = store.ReadPublished(actor.id);
  ASSERT_TRUE(pub);
  EXPECT_DOUBLE_EQ(pub->x, 10);

  ASSERT_TRUE(store.Despawn(actor.id));
  EXPECT_FALSE(store.Exists(actor.id));
  ASSERT_TRUE(store.Despawn(client.id));
}

TEST(SpatialObjectBackend, SwapMapBackend)
{
  LockFreeSpatialObjectIndex index(64);
  MapComponentBackend backend;
  SpatialObjectStore store(index, backend);
  ASSERT_TRUE(store.SpawnActor(SpawnActorRequest{
      .id = MakeId(5), .info = {}, .transform = Transform{.x = 1}}));
  EXPECT_TRUE(store.IsActor(MakeId(5)));
  auto t = store.GetTransform(MakeId(5));
  ASSERT_TRUE(t);
  EXPECT_DOUBLE_EQ(t->x, 1);
}

TEST(SpatialObjectCmdSig, SerializeRoundTrip)
{
  TransitCommandEnvelope cmd;
  cmd.targetActor = MakeId(11);
  cmd.ordering = OrderingKey{MakeId(11), 3};
  cmd.package.commandPayload.commandName = "example.Move";
  cmd.package.commandPayload.payload = {1, 2, 3};
  cmd.package.deliveryMode = CommandDeliveryGuarantee::Reliable;

  NetBinaryWriter writer;
  writer(cmd);
  NetBinaryReader reader(writer.GetBytes());
  TransitCommandEnvelope out;
  reader(out);
  EXPECT_EQ(out.targetActor, cmd.targetActor);
  EXPECT_EQ(out.ordering.sequence, 3u);
  EXPECT_EQ(std::string(out.package.commandPayload.commandName.c_str()),
            "example.Move");
  EXPECT_EQ(out.package.commandPayload.payload.size(), 3u);

  TransitSignalEnvelope sig;
  sig.targetClient = MakeId(9);
  sig.sourceActor = MakeId(11);
  sig.ordering = cmd.ordering;
  sig.payload.signalName = "example.Moved";
  NetBinaryWriter w2;
  w2(sig);
  NetBinaryReader r2(w2.GetBytes());
  TransitSignalEnvelope sigOut;
  r2(sigOut);
  EXPECT_EQ(sigOut.targetClient, sig.targetClient);
  EXPECT_EQ(std::string(sigOut.payload.signalName.c_str()), "example.Moved");
}

TEST(SpatialObjectCmdSig, ActorTargetRequired)
{
  struct FakeStore : ISpatialObjectStore
  {
    bool SpawnActor(const SpawnActorRequest&) override
    {
      return false;
    }
    bool SpawnClientActor(const SpawnClientActorRequest&) override
    {
      return false;
    }
    bool Despawn(const AtlasNetEntityID&) override
    {
      return false;
    }
    bool UpdateTransform(const AtlasNetEntityID&, const Transform&) override
    {
      return false;
    }
    bool Exists(const AtlasNetEntityID&) const override
    {
      return true;
    }
    bool IsActor(const AtlasNetEntityID&) const override
    {
      return false;
    }
    bool IsClient(const AtlasNetEntityID&) const override
    {
      return false;
    }
    std::optional<PublishedState>
    ReadPublished(const AtlasNetEntityID&) const override
    {
      return std::nullopt;
    }
    std::optional<Transform> GetTransform(const AtlasNetEntityID&) const override
    {
      return std::nullopt;
    }
    std::optional<ObjectInfo>
    GetObjectInfo(const AtlasNetEntityID&) const override
    {
      return std::nullopt;
    }
    std::optional<uint64_t> NextCommandSequence(const AtlasNetEntityID&) override
    {
      return std::nullopt;
    }
    IComponentBackend& Backend() override
    {
      return backend;
    }
    const IComponentBackend& Backend() const override
    {
      return backend;
    }
    MapComponentBackend backend;
  };

  FakeStore store;
  const AtlasNetNodeID node = UUID::Generate();
  struct AlwaysLocal : IEntityOwnershipSource
  {
    AtlasNetNodeID node;
    explicit AlwaysLocal(AtlasNetNodeID n) : node(n) {}
    std::optional<AtlasNetNodeID>
    OwnerOfEntity(const AtlasNetEntityID&) const override
    {
      return node;
    }
    std::optional<AtlasNetNodeID>
    OwnerOfClient(const AtlasNetClientID&) const override
    {
      return node;
    }
  } ownership(node);

  CommandDispatcher dispatcher(CommandDispatcher::Config{
      .store = &store,
      .ownership = &ownership,
      .localNodeId = node});

  TransitCommandEnvelope env;
  env.targetActor = MakeId(50);
  env.package.commandPayload.commandName = "example.Move";
  auto ack = dispatcher.Dispatch(env);
  EXPECT_EQ(ack.status, CommandAckStatus::NotActor);
}

TEST(SpatialObjectCmdSig, DispatchLocalAndOrdering)
{
  LockFreeSpatialObjectIndex index(64);
  EnttComponentBackend backend;
  const AtlasNetNodeID node = UUID::Generate();
  SpatialObjectStore store(index, backend, node);
  StoreOwnershipSource ownership(store, node);
  CapturingSignalEgress egress;
  CapturingCommandTransport transport;

  ASSERT_TRUE(store.SpawnActor(SpawnActorRequest{
      .id = MakeId(1),
      .info = ObjectInfo{.worldId = node},
      .transform = {}}));

  CommandDispatcher dispatcher(CommandDispatcher::Config{
      .store = &store,
      .ownership = &ownership,
      .transport = &transport,
      .signalEgress = &egress,
      .localNodeId = node});

  dispatcher.Bind(
      "example.Move",
      [](ISpatialObjectStore& s, const TransitCommandEnvelope& env,
         TransitSignalEnvelope* outSignal) -> bool
      {
        Transform t{.x = static_cast<double>(env.ordering.sequence)};
        if (!s.UpdateTransform(env.targetActor, t))
        {
          return false;
        }
        outSignal->targetClient = MakeId(1);
        outSignal->payload.signalName = "example.Moved";
        return true;
      });

  std::vector<uint64_t> sequences;
  for (int i = 0; i < 5; ++i)
  {
    TransitCommandEnvelope env;
    env.targetActor = MakeId(1);
    env.package.commandPayload.commandName = "example.Move";
    auto ack = dispatcher.Dispatch(env);
    ASSERT_EQ(ack.status, CommandAckStatus::Ok);
    sequences.push_back(ack.ordering.sequence);
  }
  for (std::size_t i = 1; i < sequences.size(); ++i)
  {
    EXPECT_GT(sequences[i], sequences[i - 1]);
  }

  auto signals = egress.TakeEmitted();
  ASSERT_EQ(signals.size(), 5u);
  for (std::size_t i = 0; i < signals.size(); ++i)
  {
    EXPECT_EQ(signals[i].ordering.sequence, sequences[i]);
    EXPECT_EQ(std::string(signals[i].payload.signalName.c_str()),
              "example.Moved");
  }

  auto pub = store.ReadPublished(MakeId(1));
  ASSERT_TRUE(pub);
  EXPECT_DOUBLE_EQ(pub->x, static_cast<double>(sequences.back()));
  EXPECT_EQ(pub->commandSequence, sequences.back());
}

TEST(SpatialObjectCmdSig, AtomicPublish)
{
  LockFreeSpatialObjectIndex index(64);
  EnttComponentBackend backend;
  const AtlasNetNodeID node = UUID::Generate();
  SpatialObjectStore store(index, backend, node);
  StoreOwnershipSource ownership(store, node);
  CapturingSignalEgress egress;

  ASSERT_TRUE(store.SpawnActor(
      SpawnActorRequest{.id = MakeId(3), .info = {}, .transform = {}}));

  CommandDispatcher dispatcher(CommandDispatcher::Config{
      .store = &store,
      .ownership = &ownership,
      .signalEgress = &egress,
      .localNodeId = node});

  dispatcher.Bind(
      "example.Move",
      [](ISpatialObjectStore& s, const TransitCommandEnvelope& env,
         TransitSignalEnvelope* out) -> bool
      {
        Transform t{.x = 100, .y = 100, .z = 100};
        EXPECT_TRUE(s.UpdateTransform(env.targetActor, t));
        out->payload.signalName = "done";
        return true;
      });

  std::atomic<bool> stop{false};
  std::thread reader(
      [&]()
      {
        while (!stop.load(std::memory_order_relaxed))
        {
          if (auto p = store.ReadPublished(MakeId(3)))
          {
            EXPECT_DOUBLE_EQ(p->x, p->y);
            EXPECT_DOUBLE_EQ(p->y, p->z);
          }
        }
      });

  for (int i = 0; i < 200; ++i)
  {
    TransitCommandEnvelope env;
    env.targetActor = MakeId(3);
    env.package.commandPayload.commandName = "example.Move";
    ASSERT_EQ(dispatcher.Dispatch(env).status, CommandAckStatus::Ok);
  }
  stop = true;
  reader.join();
}

TEST(SpatialObjectCmdSig, TransportForward)
{
  LockFreeSpatialObjectIndex index(64);
  EnttComponentBackend backend;
  const AtlasNetNodeID node = UUID::Generate();
  SpatialObjectStore store(index, backend, node);
  StoreOwnershipSource ownership(store, node);
  CapturingCommandTransport transport;
  CapturingSignalEgress egress;

  CommandDispatcher dispatcher(CommandDispatcher::Config{
      .store = &store,
      .ownership = &ownership,
      .transport = &transport,
      .signalEgress = &egress,
      .localNodeId = node});

  TransitCommandEnvelope env;
  env.targetActor = MakeId(999);
  env.package.commandPayload.commandName = "example.Move";
  auto ack = dispatcher.Dispatch(env);
  EXPECT_EQ(ack.status, CommandAckStatus::Forwarded);
  auto forwarded = transport.TakeForwarded();
  ASSERT_EQ(forwarded.size(), 1u);
  EXPECT_EQ(forwarded[0].targetActor, MakeId(999));
}

TEST(SpatialObjectOwnership, ResolverLocalAndUnknown)
{
  LockFreeSpatialObjectIndex index(64);
  EnttComponentBackend backend;
  const AtlasNetNodeID node = UUID::Generate();
  SpatialObjectStore store(index, backend, node);
  StoreOwnershipSource ownership(store, node);
  Network::Intent::ClusterIntentResolver resolver(&ownership);

  ASSERT_TRUE(store.SpawnActor(
      SpawnActorRequest{.id = MakeId(1), .info = {}, .transform = {}}));

  auto local = resolver.ResolveIntent(
      Network::Intent::Recepient::ShardOfEntityRecepient{.entityID = MakeId(1)});
  ASSERT_TRUE(local);
  EXPECT_EQ(*local, node);

  auto missing = resolver.ResolveIntent(
      Network::Intent::Recepient::ShardOfEntityRecepient{.entityID = MakeId(2)});
  EXPECT_FALSE(missing);

  auto direct = resolver.ResolveIntent(
      Network::Intent::Recepient::NodeRecepient{.nodeID = node});
  ASSERT_TRUE(direct);
  EXPECT_EQ(*direct, node);
}
