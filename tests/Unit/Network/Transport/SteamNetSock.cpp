#include "AtlasNet/Core/Network/Transport/Connection/SteamNetSock/SteamNetSock.hpp"
#include "Commons.hpp"
#include <gtest/gtest.h>
using namespace AtlasNet::Network;
TEST(SteamNetSock, Init)
{
  SteamNetSockTransport transport;
}
TEST(SteamNetSock, Listen)
{
  SteamNetSockTransport transport;
  int port = pick_available_port();
  ASSERT_NE(port, -1) << "No available port found for testing.";
  SocketAddress address(HostAddress("127.0.0.1"), port);
  auto listener = transport.Listen(address);

  EXPECT_NE(listener, nullptr) << "Failed to create listener on port " << port;
}
TEST(SteamNetSock, ListenAndConnect)
{
  SteamNetSockTransport transport;
  int port = pick_available_port();
  ASSERT_NE(port, -1) << "No available port found for testing.";
  SocketAddress address(HostAddress("127.0.0.1"), port);
  auto listener = transport.Listen(address);

  std::condition_variable cv;
  std::mutex mtx;
  std::condition_variable cv2;
  std::mutex mtx2;
  std::atomic_bool serverConnectionEstablished{false},
      clientConnectionEstablished{false};
  EXPECT_NE(listener, nullptr) << "Failed to create listener on port " << port;
  std::shared_ptr<IConnection> ListenerConnection;
  listener->SetConnectionRequestCallback(
      [&](ConnectionRequest& request, IConnectionListener& listener)
      {
        std::cout << "Connection request from: "
                  << request.RemoteAddress().to_string() << std::endl;
        ListenerConnection = request.Accept();
        ListenerConnection->OnStateChange(
            [&](ConnectionState state)
            {
              if (state == ConnectionState::Connected)
              {
                serverConnectionEstablished = true;
                std::lock_guard<std::mutex> lock(mtx2);
                cv2.notify_one();
              }
            });
      });
  auto connection = transport.Connect(address);
  connection->OnStateChange(
      [&](ConnectionState state)
      {
        if (state == ConnectionState::Connected)
        {
          clientConnectionEstablished = true;
          std::lock_guard<std::mutex> lock(mtx);
          cv.notify_one();
        }
      });
  EXPECT_NE(connection, nullptr)
      << "Failed to connect to listener on port " << port;

  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return clientConnectionEstablished.load(); });
    EXPECT_EQ(connection->GetState(), ConnectionState::Connected)
        << "Connection did not reach connected state within timeout.";
  }
  {
    std::unique_lock<std::mutex> lock(mtx2);
    cv2.wait(lock, [&] { return serverConnectionEstablished.load(); });
    EXPECT_EQ(ListenerConnection->GetState(), ConnectionState::Connected)
        << "Listener connection did not reach connected state within timeout.";
  }
}

TEST(SteamNetSock, SendRecv)
{
  SteamNetSockTransport transport;
  int port = pick_available_port();
  ASSERT_NE(port, -1) << "No available port found for testing.";
  SocketAddress address(HostAddress("127.0.0.1"), port);
  auto listener = transport.Listen(address);

  std::condition_variable cv;
  std::mutex mtx;
  std::condition_variable cv2;
  std::mutex mtx2;
  std::atomic_bool serverConnectionEstablished{false},
      clientConnectionEstablished{false};
  EXPECT_NE(listener, nullptr) << "Failed to create listener on port " << port;
  std::shared_ptr<IConnection> serverConnection;
  listener->SetConnectionRequestCallback(
      [&](ConnectionRequest& request, IConnectionListener& listener)
      {
        std::cout << "Connection request from: "
                  << request.RemoteAddress().to_string() << std::endl;
        serverConnection = request.Accept();
        serverConnection->OnStateChange(
            [&](ConnectionState state)
            {
              if (state == ConnectionState::Connected)
              {
                std::cout << "Listener connection established with "
                          << serverConnection->RemoteAddress().to_string()
                          << std::endl;
                serverConnectionEstablished = true;
                std::lock_guard<std::mutex> lock(mtx);
                cv2.notify_one();
              }
            });
      });
  auto clientConnection = transport.Connect(address);
  EXPECT_NE(clientConnection, nullptr)
      << "Failed to connect to listener on port " << port;

  clientConnection->OnStateChange(
      [&](ConnectionState state)
      {
        if (state == ConnectionState::Connected)
        {
          clientConnectionEstablished = true;
          std::lock_guard<std::mutex> lock(mtx);
          cv.notify_one();
        }
      });
  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return clientConnectionEstablished.load(); });
    EXPECT_EQ(clientConnection->GetState(), ConnectionState::Connected)
        << "Connection did not reach connected state within timeout.";
  }
  {
    std::unique_lock<std::mutex> lock(mtx2);
    cv2.wait(lock, [&] { return serverConnectionEstablished.load(); });
    EXPECT_EQ(serverConnection->GetState(), ConnectionState::Connected)
        << "Listener connection did not reach connected state within timeout.";
  }


  serverConnection->Send(
      std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>("Hello from server"), 17),
      PacketSendMode::Reliable);
  clientConnection->Send(
      std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>("Hello from client"), 17),
      PacketSendMode::Reliable);
  
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for packets to be sent
  std::vector<Packet> packets(10);
  size_t clientReceived = clientConnection->TryReceive(std::span<Packet>(packets));
  size_t serverReceived = serverConnection->TryReceive(std::span<Packet>(packets.begin() + clientReceived, packets.end()));
  EXPECT_GT(clientReceived, 0) << "Client did not receive any packets.";
  EXPECT_GT(serverReceived, 0) << "Server did not receive any packets.";

  EXPECT_EQ(std::string(packets[0].Payload().begin(), packets[0].Payload().end()), "Hello from server")
      << "Client received unexpected packet content.";
  EXPECT_EQ(std::string(packets[clientReceived].Payload().begin(), packets[clientReceived].Payload().end()), "Hello from client")
      << "Server received unexpected packet content.";

}
