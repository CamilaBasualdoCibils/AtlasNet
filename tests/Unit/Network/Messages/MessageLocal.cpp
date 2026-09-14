
#include "AtlasNet/Core/Network/Messages/Message.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <gtest/gtest.h>
#include <mutex>
#include <netinet/in.h>
#include <string>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
using namespace AtlasNet;
using namespace AtlasNet::Network;
struct Payload
{
  int a;
  std::string b;
  std::wstring c;
  boost::container::small_vector<int, 4> d;
  std::bitset<32> e;
  std::array<int, 4> f;
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(a, b, c, d, e, f);
  }
};
using TestMessage =
    AtlasNet::Network::Messages::Message<"Test Message", Payload>;
TEST(MessageSystem, Serialization)
{

  Payload payload{42,           "Hello, AtlasNet!",        L"Hello, AtlasNet!",
                  {1, 2, 3, 4}, std::bitset<32>(0b101010), {1, 2, 3, 4}};

  NetBinaryWriter writer;
  TestMessage msg{payload};
  msg.serialize(writer);
  TestMessage deserializedMsg;
  NetBinaryReader reader(writer.GetBytes());
  deserializedMsg.serialize(reader);
  EXPECT_EQ(deserializedMsg.payload.a, payload.a);
  EXPECT_EQ(deserializedMsg.payload.b, payload.b);
  EXPECT_EQ(deserializedMsg.payload.c, payload.c);
  EXPECT_EQ(deserializedMsg.payload.d, payload.d);
  EXPECT_EQ(deserializedMsg.payload.e, payload.e);
  EXPECT_EQ(deserializedMsg.payload.f, payload.f);
}
/* TEST(MessageSystem, InitAndShutdown)
{

  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});

  SUCCEED();
}
TEST(MessageSystem, OpenListenSocket)
{
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});

  const PortType port = pick_available_port();
  msgsys.OpenListenSocket(port);

  SUCCEED();
}
ATLASNET_MESSAGE(IOTestMessage, ATLASNET_MESSAGE_DATA(int, intVal),
                 ATLASNET_MESSAGE_DATA(std::string, strVal))
TEST(MessageSystem, MessageIO)
{
  using namespace AtlasNet;
  IOTestMessage msg;
  msg.intVal = 123;
  msg.strVal = "Hello, AtlasNet!";
  ByteWriter writer;
  msg.Serialize(writer);
  auto bytes = writer.bytes();

  ByteReader readerID(bytes);
  MessageID typeHash = IMessage::DeserializeTypeIdHash(readerID);
  EXPECT_EQ(typeHash, IOTestMessage::TypeIdHash);
  ByteReader reader(bytes);
  IOTestMessage deserializedMsg;
  deserializedMsg.Deserialize(reader);
  EXPECT_EQ(deserializedMsg.intVal, msg.intVal);
  EXPECT_EQ(deserializedMsg.strVal, msg.strVal);
}
TEST(MessageSystem, Connect)
{
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});

  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  auto connectJob = msgsys.Connect(serverAddr);

  connectJob->wait();
  MessageConnectionResult res = connectJob->get();

  EXPECT_EQ(res.code, MessageConnectionResultCode::eSuccess);
  EXPECT_EQ(msgsys.GetNumConnections(), 2); // 2 since its local host
}
TEST(MessageSystem, MultipleConnectSameAddress)
{
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});

  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  auto connectJob = msgsys.Connect(serverAddr);

  connectJob->wait();
  MessageConnectionResult res = connectJob->get();

  EXPECT_EQ(res.code, MessageConnectionResultCode::eSuccess);
  EXPECT_EQ(msgsys.GetNumConnections(), 2);

  auto connectJob2 = msgsys.Connect(serverAddr);
  std::future_status status2 = connectJob2->wait_for(std::chrono::seconds(5));
  if (status2 == std::future_status::timeout)
  {
    FAIL() << "Connection timed out";
  }
  MessageConnectionResult res2 = connectJob2->get();
  EXPECT_EQ(res2.code, MessageConnectionResultCode::eAlreadyConnected);
  EXPECT_EQ(msgsys.GetNumConnections(), 2);
}
ATLASNET_MESSAGE(TestMessage, ATLASNET_MESSAGE_DATA(int, u8_val),
                 ATLASNET_MESSAGE_DATA(std::string, str))
TEST(MessageSystem, TrySendMessage)
{
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.Connect(serverAddr)->wait();
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  auto result = msgsys.TrySendMessage(msg, serverAddr,
                                      AtlasNet::MessageSendMode::eReliable);

  EXPECT_EQ(result.code, MessageSendResultCode::eSuccess)
      << "Failed to send message: " << static_cast<int>(result.code);
  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
};
ATLASNET_MESSAGE(messagechain1, ATLASNET_MESSAGE_DATA(int, u8_val),
                 ATLASNET_MESSAGE_DATA(std::string, str))
ATLASNET_MESSAGE(messagechain2, ATLASNET_MESSAGE_DATA(int, u8_val),
                 ATLASNET_MESSAGE_DATA(std::string, str))
TEST(MessageSystem, ChainMessage)
{
  using namespace AtlasNet;

  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  std::atomic_bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  msgsys.On<messagechain1>(
      [&](const messagechain1& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received messagechain1 from " << address.to_string();

        messagechain2 msg2;
        msg2.u8_val = msg.u8_val + 1;
        msg2.str = msg.str + " Again!";
        TaskHandle<MessageSendResult> SendJob2 = msgsys.QueueMessage(
            msg2, address, AtlasNet::MessageSendMode::eReliable);
        SendJob2->wait_for(std::chrono::seconds(5));
      });
  msgsys.On<messagechain2>(
      [&](const messagechain2& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 43);
        EXPECT_EQ(msg.str, "Hello, AtlasNet! Again!");
        SUCCEED() << "Received messagechain2 from " << address.to_string();
        success = true;
        std::lock_guard lock(mtx);
        cv.notify_one();
      });
  PortType port = pick_available_port();
  SocketAddress serverAddr(HostName("localhost"), port);
  msgsys.OpenListenSocket(port);
  TaskHandle<MessageSendResult> SendJob = msgsys.QueueMessage(
      messagechain1{.u8_val = 42, .str = "Hello, AtlasNet!"}, serverAddr,
      AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
                          [&success] { return success.load(); }))
      << "Did not receive chained message within timeout";

  EXPECT_TRUE(SendJob.GetTask().is_done())
      << "SendMessage job did not complete within timeout";
  EXPECT_TRUE(success.load())
      << "Did not receive chained message within timeout";
}

TEST(MessageSystem, SendMessageWithoutConnecting)
{
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  TaskHandle<MessageSendResult> SendJob = msgsys.QueueMessage(
      msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
};
TEST(MessageSystem, QueueMessageResponseToMessage)
{

  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  TaskHandle<MessageSendResult> SendJob = msgsys.QueueMessage(
      msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
  std::vector<MessageSystem::Connection> connections;
  msgsys.GetConnections(connections);
  EXPECT_EQ(connections.size(), 2);
  SocketAddress localInitiator =
      connections[0].GetRequestedAddress() != serverAddr
          ? connections[0].GetRequestedAddress()
          : connections[1].GetRequestedAddress();

  auto sendTask = msgsys.QueueMessage(msg, localInitiator,
                                      AtlasNet::MessageSendMode::eReliable);

  std::future_status status = sendTask->wait_for(std::chrono::seconds(5));
  if (status != std::future_status::ready)
  {
    FAIL() << "Did not send message within timeout";
  }
  EXPECT_EQ(msgsys.GetNumConnections(), 2);
}
TEST(MessageSystem, DoubleListenSocket)
{

  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  using namespace AtlasNet;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  const PortType port2 = pick_available_port();
  SocketAddress serverAddr2(dnsAddr, port2);
  msgsys.OpenListenSocket(port2);
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  TaskHandle<MessageSendResult> SendJob = msgsys.QueueMessage(
      msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
  std::vector<MessageSystem::Connection> connections;
  msgsys.GetConnections(connections);
  EXPECT_EQ(connections.size(), 2);

  auto sendTask = msgsys.QueueMessage(msg, serverAddr2,
                                      AtlasNet::MessageSendMode::eReliable);

  std::future_status status = sendTask->wait_for(std::chrono::seconds(5));
  if (status != std::future_status::ready)
  {
    FAIL() << "Did not send message within timeout";
  }
  EXPECT_EQ(msgsys.GetNumConnections(), 4);
}
TEST(MessageSystem, ListenMessagePort)
{
  using namespace AtlasNet;

  std::atomic_int PortCount = 0;
  std::atomic_int AllCount = 0;
  std::mutex mtx;
  std::condition_variable cv;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});
  const PortType port1 = pick_available_port();
  SocketAddress serverAddr(HostName("localhost"), port1);
  msgsys.OpenListenSocket(port1);

  const PortType port2 = pick_available_port();
  EXPECT_NE(port1, port2);
  SocketAddress serverAddr2(HostName("localhost"), port2);
  msgsys.OpenListenSocket(port2).On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        PortCount++;
        std::lock_guard lock(mtx);
        cv.notify_one();
      });

  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        AllCount++;
        std::lock_guard lock(mtx);
        cv.notify_one();
      });

  msgsys.Connect(serverAddr);

  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  msgsys.QueueMessage(msg, serverAddr, AtlasNet::MessageSendMode::eReliable)
      ->wait_for(std::chrono::seconds(2));
  msgsys.QueueMessage(msg, serverAddr2, AtlasNet::MessageSendMode::eReliable)
      ->wait_for(std::chrono::seconds(2));

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&]
                  { return AllCount.load() >= 2 && PortCount.load() >= 1; }))
      << "Did not receive messages within timeout";
  EXPECT_EQ(PortCount.load(), 1)
      << "Port-specific handler should have been called once";
  EXPECT_GE(AllCount.load(), 2)
      << "General handler should have been called for all messages";
}
ATLASNET_MESSAGE(BigMessageTestMessage,
                 ATLASNET_MESSAGE_DATA(std::vector<uint8_t>, data))

TEST(MessageSystem, BigMessage)
{
  using namespace AtlasNet;
  using namespace std::chrono_literals;
  TaskSystem jobsys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.taskSystem = &jobsys});

  std::vector<std::size_t> sizes = {
      1ull,     10ull,     100ull,
      1'000ull, 10'000ull, 100'000ull, // 1'000'000ull, 10'000'000ull,
                                       // 100'000'000ull,
                                       // 1'000'000'000ull
  };

  std::mutex mtx;
  std::condition_variable cv;
  std::vector<uint8_t> receivedData;

  const PortType port = pick_available_port();
  SocketAddress addr("127.0.0.1:" + std::to_string(port));

  msgsys.OpenListenSocket(port);

  msgsys.On<BigMessageTestMessage>(
      [&](const BigMessageTestMessage& msg, const SocketAddress&)
      {
        {
          std::lock_guard lock(mtx);
          receivedData = msg.data;
        }
        cv.notify_all();
      });

  auto conn = msgsys.Connect(addr);
  // ASSERT_TRUE(conn.valid());

  for (std::size_t currentSize : sizes)
  {
    std::vector<uint8_t> bigData(currentSize);
    for (std::size_t j = 0; j < currentSize; ++j)
    {
      bigData[j] = static_cast<uint8_t>(j % 256);
    }

    {
      std::lock_guard lock(mtx);
      receivedData.clear();
    }

    BigMessageTestMessage msg;
    msg.data = bigData;

    msgsys.QueueMessage(msg, addr, MessageSendMode::eReliable);

    {
      std::unique_lock lock(mtx);
      ASSERT_TRUE(
          cv.wait_for(lock, 30s, [&] { return receivedData == bigData; }))
          << "Timed out waiting for correct message of size " << currentSize
          << ", last received size was " << receivedData.size();
    }

    ASSERT_EQ(receivedData.size(), bigData.size())
        << "Size mismatch for payload size " << currentSize;

    ASSERT_EQ(receivedData, bigData)
        << "Content mismatch for payload size " << currentSize;
  }
}

TEST(MessageSystem, Handshake_Valid_Self)
{
  using namespace AtlasNet;

  auto handshakeHandler =
      [](const HandshakeIdentity& identity,
         const SocketAddress& address) -> HandshakeResponsePacket
  { //acept all
    return HandshakeResponsePacket{.accepted = true}; };

  HandshakeIdentity identity{
      .role = HandshakeRole::eClient,
      .data = HandshakeClientRequestData{.payload = {1, 2, 3, 4, 5}}};
  TaskSystem tasksys(TaskSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{
      .taskSystem = &tasksys,
      .handshakeHandler = handshakeHandler,
      .handshakeIdentity = identity,
  });

  PortType port = pick_available_port();
  msgsys.OpenListenSocket(port);
  SocketAddress addr(HostName("localhost"), port);
  TaskHandle<MessageConnectionResult> connectionJob = msgsys.Connect(addr);
  connectionJob->wait_for(std::chrono::seconds(5));

  EXPECT_TRUE(connectionJob.GetTask().is_done());
  EXPECT_EQ(msgsys.GetNumConnections(), 2);
  EXPECT_EQ(msgsys.GetConnectionState(addr), ConnectionState::eConnected);
}
TEST(MessageSystem, Handshake_Valid_Fork)
{

  using namespace AtlasNet;
  auto handshakeHandler =
      [](const HandshakeIdentity& identity,
         const SocketAddress& address) -> HandshakeResponsePacket
  { //acept all
    return HandshakeResponsePacket{.accepted = true}; };

  const PortType parentPort = pick_available_port();
  const SocketAddress address(HostName("localhost"), parentPort);
  int readyPipe[2];
  ASSERT_EQ(pipe(readyPipe), 0) << "Failed to create pipe";

  const int expectedResult =
      std::chrono::system_clock::now().time_since_epoch().count() %
      10000; // Just some arbitrary value to return from child to parent
  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork() failed";
  if (pid == 0)
  {
    // Child process: hosts RPC server on childPort.
    close(readyPipe[0]);

    TaskSystem childTaskSystem(TaskSystem::Config{});
    HandshakeIdentity childidentity{
        .role = HandshakeRole::eClient,
        .data = HandshakeClientRequestData{.payload = {1, 2, 3, 4, 5}}};
    MessageSystem childMsgSystem(
        MessageSystem::Config{.taskSystem = &childTaskSystem,
                              .handshakeIdentity = childidentity,
                              .handshakeHandler = handshakeHandler});
    // Signal readiness to parent.
    const uint8_t ready = 1;
    (void)write(readyPipe[1], &ready, 1);
    close(readyPipe[1]);

    TaskHandle<MessageConnectionResult> connectJob =
        childMsgSystem.Connect(address);
    connectJob->wait_for(std::chrono::seconds(2));
    if (!connectJob.GetTask().is_done() ||
        childMsgSystem.GetConnectionState(address) !=
            ConnectionState::eConnected)
    {
      std::cerr << "Child failed to connect to parent within timeout\n";
      _exit(1);
    }
    _exit(0);
  }

  close(readyPipe[1]);

  uint8_t ready = 0;
  ASSERT_EQ(read(readyPipe[0], &ready, 1), 1)
      << "Parent failed waiting for child readiness";
  close(readyPipe[0]);

  TaskSystem parentTaskSystem(TaskSystem::Config{});
  HandshakeIdentity parentIdentity{
      .role = HandshakeRole::eServer,
      .data = HandshakeServerRequestData{
          .serviceID = {},
          .serviceType = AtlasNetNodeType::Controller}}; // Just some dummy
                                                         // handshake identity
  MessageSystem parentMsgSystem(
      MessageSystem::Config{.taskSystem = &parentTaskSystem,
                            .handshakeIdentity = parentIdentity,
                            .handshakeHandler = handshakeHandler});
  parentMsgSystem.OpenListenSocket(parentPort);

  int childStatus = 0;
  ASSERT_EQ(waitpid(pid, &childStatus, 0), pid);
  ASSERT_TRUE(WIFEXITED(childStatus));
  EXPECT_EQ(WEXITSTATUS(childStatus), 0);
}

TEST(MessageSystem, Handshake_Invalid_Client_Fork)
{

  using namespace AtlasNet;
  auto handshakeHandler =
      [](const HandshakeIdentity& identity,
         const SocketAddress& address) -> HandshakeResponsePacket
  { //acept all
    if (identity.role == HandshakeRole::eClient)
    {
      return HandshakeResponsePacket{.accepted = false,
                                     .rejectReason = "Clients not allowed"};
    }
    return HandshakeResponsePacket{.accepted = true}; };

  const PortType parentPort = pick_available_port();
  const SocketAddress address(HostName("localhost"), parentPort);
  int readyPipe[2];
  ASSERT_EQ(pipe(readyPipe), 0) << "Failed to create pipe";

  const int expectedResult =
      std::chrono::system_clock::now().time_since_epoch().count() %
      10000; // Just some arbitrary value to return from child to parent
  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork() failed";
  if (pid == 0)
  {
    // Child process: hosts RPC server on childPort.
    close(readyPipe[0]);

    TaskSystem childTaskSystem(TaskSystem::Config{});
    HandshakeIdentity childidentity{
        .role = HandshakeRole::eClient,
        .data = HandshakeClientRequestData{.payload = {1, 2, 3, 4, 5}}};
    MessageSystem childMsgSystem(
        MessageSystem::Config{.taskSystem = &childTaskSystem,
                              .handshakeIdentity = childidentity,
                              .handshakeHandler = handshakeHandler});
    // Signal readiness to parent.
    const uint8_t ready = 1;
    (void)write(readyPipe[1], &ready, 1);
    close(readyPipe[1]);

    TaskHandle<MessageConnectionResult> connectJob =
        childMsgSystem.Connect(address);
    connectJob->wait_for(std::chrono::seconds(2));
    if (!connectJob.GetTask().is_done() ||
        childMsgSystem.GetConnectionState(address) !=
            ConnectionState::eConnected)
    {
      std::cerr << "Child failed to connect to parent within timeout\n";
      _exit(1);
    }
    _exit(0);
  }

  close(readyPipe[1]);

  uint8_t ready = 0;
  ASSERT_EQ(read(readyPipe[0], &ready, 1), 1)
      << "Parent failed waiting for child readiness";
  close(readyPipe[0]);

  TaskSystem parentTaskSystem(TaskSystem::Config{});
  HandshakeIdentity parentIdentity{
      .role = HandshakeRole::eServer,
      .data = HandshakeServerRequestData{
          .serviceID = {},
          .serviceType = AtlasNetNodeType::Controller}}; // Just some dummy
                                                         // handshake identity
  MessageSystem parentMsgSystem(
      MessageSystem::Config{.taskSystem = &parentTaskSystem,
                            .handshakeIdentity = parentIdentity,
                            .handshakeHandler = handshakeHandler});
  parentMsgSystem.OpenListenSocket(parentPort);

  // we expect a failure from child
  int childStatus = 0;
  ASSERT_EQ(waitpid(pid, &childStatus, 0), pid);
  ASSERT_TRUE(WIFEXITED(childStatus));
  EXPECT_EQ(WEXITSTATUS(childStatus), 1);
}
TEST(MessageSystem, Handshake_Invalid_Server_Fork)
{

  using namespace AtlasNet;
  auto handshakeHandler =
      [](const HandshakeIdentity& identity,
         const SocketAddress& address) -> HandshakeResponsePacket
  { //acept all
    if (identity.role == HandshakeRole::eServer)
    {
      return HandshakeResponsePacket{.accepted = false,
                                     .rejectReason = "Servers not allowed"};
    }
    return HandshakeResponsePacket{.accepted = true}; };

  const PortType parentPort = pick_available_port();
  const SocketAddress address(HostName("localhost"), parentPort);
  int readyPipe[2];
  ASSERT_EQ(pipe(readyPipe), 0) << "Failed to create pipe";

  const int expectedResult =
      std::chrono::system_clock::now().time_since_epoch().count() %
      10000; // Just some arbitrary value to return from child to parent
  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork() failed";
  if (pid == 0)
  {
    // Child process: hosts RPC server on childPort.
    close(readyPipe[0]);

    TaskSystem childTaskSystem(TaskSystem::Config{});
    HandshakeIdentity childidentity{
        .role = HandshakeRole::eClient,
        .data = HandshakeClientRequestData{.payload = {1, 2, 3, 4, 5}}};
    MessageSystem childMsgSystem(
        MessageSystem::Config{.taskSystem = &childTaskSystem,
                              .handshakeIdentity = childidentity,
                              .handshakeHandler = handshakeHandler});
    // Signal readiness to parent.
    const uint8_t ready = 1;
    (void)write(readyPipe[1], &ready, 1);
    close(readyPipe[1]);

    TaskHandle<MessageConnectionResult> connectJob =
        childMsgSystem.Connect(address);
    connectJob->wait_for(std::chrono::seconds(2));
    if (!connectJob.GetTask().is_done() ||
        childMsgSystem.GetConnectionState(address) !=
            ConnectionState::eConnected)
    {
      std::cerr << "Child failed to connect to parent within timeout\n";
      _exit(1);
    }
    _exit(0);
  }

  close(readyPipe[1]);

  uint8_t ready = 0;
  ASSERT_EQ(read(readyPipe[0], &ready, 1), 1)
      << "Parent failed waiting for child readiness";
  close(readyPipe[0]);

  TaskSystem parentTaskSystem(TaskSystem::Config{});
  HandshakeIdentity parentIdentity{
      .role = HandshakeRole::eServer,
      .data = HandshakeServerRequestData{
          .serviceID = {},
          .serviceType = AtlasNetNodeType::Controller}}; // Just some dummy
                                                         // handshake identity
  MessageSystem parentMsgSystem(
      MessageSystem::Config{.taskSystem = &parentTaskSystem,
                            .handshakeIdentity = parentIdentity,
                            .handshakeHandler = handshakeHandler});
  parentMsgSystem.OpenListenSocket(parentPort);

  // we expect a failure from child
  int childStatus = 0;
  ASSERT_EQ(waitpid(pid, &childStatus, 0), pid);
  ASSERT_TRUE(WIFEXITED(childStatus));
  EXPECT_EQ(WEXITSTATUS(childStatus), 1);
} */