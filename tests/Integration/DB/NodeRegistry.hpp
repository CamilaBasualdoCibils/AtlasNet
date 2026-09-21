
#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/Address/Address.hpp"
#include "AtlasNet/Core/Network/Address/MacAddress.hpp"
#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/NetworkTransportRPC.hpp"
#include "AtlasNet/Core/Network/Transport/UDP/UDPNetworkTransport.hpp"
#include "AtlasNet/DB/Handshake.hpp"
#include <algorithm>
#include <boost/describe/enum_to_string.hpp>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <thread>
using namespace AtlasNet;
class ValkeyIntegrationTest : public ::testing::Test
{
protected:
  static constexpr uint16_t valkeyPort = 16379;
  static constexpr uint16_t dbHandshakePort = ATLASNET_DB_DEBUG_HANDSHAKE_PORT;
  static constexpr uint16_t testListenPort = 0;
  static inline pid_t valkeyPid = -1;
  static inline std::shared_ptr<Network::UDPNetworkTransport>
      udpNetworkTransport;
  static inline std::shared_ptr<Network::RPC::NetworkTransportRPC> handshakeRPC;
  static void SetUpTestSuite()
  {
    udpNetworkTransport = std::make_shared<Network::UDPNetworkTransport>(
        "DB-Valkey Test Network Transport",
        Network::SocketAddress(Network::IPv6::Any(), testListenPort));

    handshakeRPC = std::make_shared<Network::RPC::NetworkTransportRPC>(
        "DB-Handshake RPC",
        Network::RPC::NetworkTransportRPC::Config{udpNetworkTransport});
    StartValkey();
    WaitForValkey();
  }

  static void TearDownTestSuite()
  {
    StopValkey();
  }

  static auto GetNetworkTransport()
  {
    return udpNetworkTransport;
  }
  static auto GetNetworkRPC()
  {
    return handshakeRPC;
  }
  static auto GetClientAddress()
  {
    return udpNetworkTransport->GetListenAddress();
  }
  static auto GetValkeyAddress()
  {
    return Network::SocketAddress(Network::IPv6::Loopback(), valkeyPort);
  }
  static auto GetAtlasNetDBAddress()
  {
    return Network::SocketAddress(Network::IPv6::Loopback(), dbHandshakePort);
  }

private:
  static void StartValkey()
  {
    valkeyPid = fork();

    if (valkeyPid < 0)
    {
      throw std::runtime_error("fork() failed");
    }

    if (valkeyPid == 0)
    {
      execl(ATLAS_TEST_VALKEY_SERVER, ATLAS_TEST_VALKEY_SERVER,

            "--port", "16379",

            "--save", "",

            "--appendonly", "no",

            "--loadmodule", ATLAS_TEST_DB_MODULE,

            nullptr);

      // Only reached if exec failed.
      std::perror("execl");
      _exit(127);
    }
  }

  static void WaitForValkey()
  {
    using namespace std::chrono_literals;

    constexpr auto timeout = 5s;
    constexpr auto retryInterval = 20ms;

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
      handshakeRPC->Poll(Network::PollType::NonBlocking);
      if (IsValkeyReady())
        return;

      // Detect child dying during startup.
      int status = 0;

      const pid_t result = waitpid(valkeyPid, &status, WNOHANG);

      if (result == valkeyPid)
      {
        valkeyPid = -1;

        throw std::runtime_error("Valkey exited during startup");
      }

      std::this_thread::sleep_for(retryInterval);
    }
    StopValkey();

    throw std::runtime_error("Timed out waiting for Valkey");
  }

  static bool IsValkeyReady()
  {
    // Use your existing AtlasNet/Valkey client here.
    //
    // Ideally this calls atlas.ping rather than merely
    // checking whether the TCP port is open.

    try
    {
      spdlog::info("Pinging DB... ");

      auto pingResult = handshakeRPC->Call<DB::RPC_DB_Ping>(
          Network::SocketAddress(Network::IPv6::Loopback(), dbHandshakePort),
          0);
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(1);
      std::future_status status = pingResult.wait_for(std::chrono::seconds(0));
      while (status != std::future_status::ready &&
             std::chrono::steady_clock::now() < deadline)
      {
        handshakeRPC->Poll(Network::PollType::NonBlocking);
        status = pingResult.wait_until(
            std::min(deadline, std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(1)));
      }
      bool Successful = status == std::future_status::ready;
      if (Successful)
        spdlog::info("DB ping successful");
      else
        spdlog::warn("DB ping failed");
      // Example:
      //
      // AtlasDB db("127.0.0.1", Port);
      // return db.Ping();

      return Successful;
    }
    catch (...)
    {
      return false;
    }
  }

  static void StopValkey()
  {
    if (valkeyPid <= 0)
      return;

    const auto waitForExit = [](std::chrono::milliseconds timeout)
    {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      do
      {
        int status = 0;
        const pid_t result = waitpid(valkeyPid, &status, WNOHANG);
        if (result == valkeyPid || (result == -1 && errno == ECHILD))
        {
          valkeyPid = -1;
          return true;
        }
        if (result == -1 && errno != EINTR)
        {
          ADD_FAILURE() << "waitpid failed: " << std::strerror(errno);
          return false;
        }
        if (std::chrono::steady_clock::now() >= deadline)
          return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (true);
    };

    // Reap an already exited child before sending any signals.
    if (waitForExit(std::chrono::milliseconds(0)))
      return;

    if (kill(valkeyPid, SIGTERM) == -1 && errno != ESRCH)
      ADD_FAILURE() << "SIGTERM failed: " << std::strerror(errno);
    if (waitForExit(std::chrono::seconds(2)))
      return;

    spdlog::warn("Valkey did not exit after SIGTERM; sending SIGKILL");
    if (kill(valkeyPid, SIGKILL) == -1 && errno != ESRCH)
      ADD_FAILURE() << "SIGKILL failed: " << std::strerror(errno);
    if (!waitForExit(std::chrono::seconds(2)))
      ADD_FAILURE() << "Valkey did not exit within 2 seconds after SIGKILL (PID "
                    << valkeyPid << ")";
  }
};

TEST_F(ValkeyIntegrationTest, Init)
{
  // AtlasDB db("127.0.0.1", Port);

  // EXPECT_TRUE(db.Ping());
}

TEST_F(ValkeyIntegrationTest, RegisterNode)
{
  DB::RegisterNodeRequest request;
  request.nodeID = AtlasNetNodeID::Generate();
  request.channelBusAddress = Network::SocketAddress("127.0.0.1:1000");
  request.handshakeAddress = GetClientAddress();
  request.macAddress = Network::MACAddress();
  auto result = GetNetworkRPC()->Call<DB::RPC_DB_RegisterNode>(
      GetAtlasNetDBAddress(), request);

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  std::future_status status = result.wait_for(std::chrono::seconds(0));
  while (status != std::future_status::ready &&
         std::chrono::steady_clock::now() < deadline)
  {
    GetNetworkRPC()->Poll(Network::PollType::NonBlocking);
    status = result.wait_until(
        std::min(deadline, std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(1)));
  }
  ASSERT_EQ(status, std::future_status::ready)
      << "Timed out waiting for node registration after 3 seconds";
  const auto response = result.get();
  ASSERT_TRUE(response.has_value()) << std::format(
      "Response does not have a value. Error: {}",
      boost::describe::enum_to_string(response.error(), "Unknown"));

  const DB::RegisterNodeResponse responseValue = response.value();
  EXPECT_EQ(responseValue,DB::RegisterNodeResponse::SUCCESS);
}
