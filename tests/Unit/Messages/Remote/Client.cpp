#pragma once

#include "CrashStack.hpp"
#include "Messages.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>
#include <variant>
std::optional<EndPointAddress> serverAddr;
int test0()
{

  std::cout << "Connecting to server at " << serverAddr->to_string()
            << std::endl;
  if (!serverAddr.has_value())
  {
    throw std::runtime_error("Server address not provided");
  }
  if (serverAddr->IsDNS())
  {
    auto resolved = serverAddr->get_dns().resolve();
    if (resolved.has_value())
    {
      std::visit(
          [](const auto& addr)
          {
            std::cout << "Resolved server address: " << addr.to_string()
                      << std::endl;
          },
          *resolved);
    }
    else
    {
      std::cerr << "Failed to resolve DNS address: "
                << serverAddr->get_dns().get_hostname() << std::endl;
    }
   
  }
  else
  {
    std::cout << "Server address: " << serverAddr->to_string() << std::endl;
  }
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();
  AtlasNet::MessageSystem::Get().Connect(serverAddr.value());

  while (AtlasNet::MessageSystem::Get().GetNumConnections() == 0)
  {
    if (std::optional<AtlasNet::MessageSystem::Connection> conn =
            AtlasNet::MessageSystem::Get().GetConnection(serverAddr.value());
        conn.has_value())
    {
      if (conn->GetState() == AtlasNet::ConnectionState::eConnected)
      {
        std::cerr << "Successfully connected to server at "
                  << serverAddr->to_string() << std::endl;
        break;
      }
    }
    std::cerr << std::format("Waiting for connection to server at {}...",
                             serverAddr->to_string())
              << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return 0;
}
int test1()
{
  if (!serverAddr.has_value())
  {
    throw std::runtime_error("Server address not provided");
  }
  if (serverAddr->IsDNS())
  {
    auto resolved = serverAddr->get_dns().resolve();
    if (resolved.has_value())
    {
      std::visit(
          [](const auto& addr)
          {
            std::cout << "Resolved server address: " << addr.to_string()
                      << std::endl;
          },
          *resolved);
    }
    else
    {
      std::cerr << "Failed to resolve DNS address: "
                << serverAddr->get_dns().get_hostname() << std::endl;
    }
  }
  else
  {
    std::cout << "Server address: " << serverAddr->to_string() << std::endl;
  }

  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();

  SimpleMessage msg;
  msg.aNumber = 42;
  msg.str = "Hello from client!";
  AtlasNet::MessageSystem::Get().SendMessage(
      msg, serverAddr.value(), AtlasNet::MessageSendMode::eReliable);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 0;
}
const static inline std::unordered_map<int, std::function<int()>> tests = {
    {0, test0}, {1, test1}};
int main(int argc, char** argv)
{

  CrashStack::Init();
  // Look for --server-addr and --test-num in args

  std::cerr << "Running Client with args: ";
  for (int i = 1; i < argc; ++i)
  {
    std::cerr << argv[i] << " ";
  }
  std::cerr << std::endl;
  std::optional<int> testNum = 0;
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--test-num" && i + 1 < argc)
    {
      testNum = std::stoi(argv[++i]);
    }
    else if (arg == "--server-addr" && i + 1 < argc)
    {
      serverAddr = EndPointAddress(argv[++i]);
    }
  }

  if (!testNum)
  {
    std::cerr << std::format("Usage: {} --test-num <num>", argv[0])
              << std::endl;
    std::string args;
    for (int i = 1; i < argc; ++i)
    {
      args += argv[i];
      args += " ";
    }
    std::cerr << std::format("Received args: {}", args) << std::endl;

    int overallResult = 0;
    for (const auto& [ID, func] : tests)
    {

      int result = func();
      overallResult += result;
      std::cerr << std::format("Test {} returned {}", ID, result) << std::endl;
    }
    return overallResult;
  }

  return tests.at(*testNum)();
}