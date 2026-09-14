#pragma once
#include "atlasnet/client/ClientRPC.hpp"
#include "atlasnet/core/CmdSig/command/Command.hpp"
#include "atlasnet/core/CmdSig/command/CommandEnums.hpp"
#include "atlasnet/core/RPC/RPCSystem.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/gateway/GatewayRPC.hpp"
#include "boost/describe/enum_to_string.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
namespace AtlasNet
{
class IAtlasNetClient
{
public:
  void AtlasNetClient_Init()
  {
    taskSystem.emplace(TaskSystem::Config{});
    messageSystem.emplace(MessageSystem::Config{
        .taskSystem = &*taskSystem,
        .handshakeIdentity =
            HandshakeIdentity{.role = HandshakeRole::eClient,
                              .data = HandshakeClientRequestData{
                                  .payload = {'H', 'e', 'l', 'l', 'o'}}}});
    rpcSystem.emplace(RPCSystem::Config{.messageSystem = &*messageSystem});

    rpcSystem->Bind<ClientRPC_ClientConnectionCompleteNotification>(
        [this](const ClientConnectionCompleteData& data)
        { OnClientConnectionCompleteNotification(data); });
  }

  enum class AtlasNetClientError
  {
    None,
    ConnectionTimedOut,
    UnknownError
  };
  bool AtlasNetClient_Connect(const std::string_view& address, uint16_t port,
                              AtlasNetClientError* error = nullptr,
                              std::string* errorMessage = nullptr)
  {
    Network::SocketAddress serverAddress(Network::HostAddress(std::string(address)), port);
    TaskHandle<MessageConnectionResult> jobHandle =
        messageSystem->Connect(serverAddress);

    jobHandle->wait_for(std::chrono::seconds(5));
    if (jobHandle.GetTask().is_done())
    {
      logger->info("Successfully connected to server at {}.",
                   serverAddress.to_string());
      logger->info("Waiting for connection complete notification...");

      std::unique_lock<std::mutex> lock(lastConnectionCompleteDataMutex);
      lastConnectionCompleteDataCV.wait_for(
          lock, std::chrono::seconds(5),
          [this]() { return lastConnectionCompleteData.has_value(); });

      if (!lastConnectionCompleteData.has_value())
      {
        if (error)
          *error = AtlasNetClientError::ConnectionTimedOut;
        if (errorMessage)
          *errorMessage = "Connection complete notification timed out.";
        return false;
      }
      logger->info("Received connection complete notification with state {}.",
                   boost::describe::enum_to_string(
                       lastConnectionCompleteData->result, "<INVALID>"));

      _serverAddress = Network::SocketAddress(Network::HostAddress(std::string(address)), port);
      return true;
    }
    else
    {
      if (error)
        *error = AtlasNetClientError::ConnectionTimedOut;
      if (errorMessage)
        *errorMessage = "Connection attempt timed out.";
      return false;
    }
  }

  CommandAckStatus AtlasNetClient_DispatchCommand(
      const std::string_view& commandName, const std::string_view& commanddata,
      CommandDeliveryGuarantee deliveryMode =
          CommandDeliveryGuarantee::GatewayConfirmedBatched,
      std::chrono::milliseconds timeout = std::chrono::seconds(5))
  {
    assert(deliveryMode != CommandDeliveryGuarantee::Invalid &&
           "Invalid send mode specified");

    IngressCommandEnvelope message;
    message.package.commandPayload.commandName.assign(commandName.data(),
                                                      commandName.size());
    message.package.commandPayload.payload.assign(commanddata.begin(),
                                                  commanddata.end());
    message.package.deliveryMode = deliveryMode;

    const bool batched =
        static_cast<uint8_t>(deliveryMode) & CommandDeliveryBatchedBit;
    const bool ackExpected =
        (deliveryMode != CommandDeliveryGuarantee::NoDelay) &&
        (deliveryMode != CommandDeliveryGuarantee::Unreliable) &&
        (deliveryMode != CommandDeliveryGuarantee::UnreliableBatched);

    if (!ackExpected)
    {
      logger->info("Dispatching command '{}' with send mode {} (batched: {}, "
                   "no ack expected)",
                   commandName,
                   boost::describe::enum_to_string(deliveryMode, "<INVALID>"),
                   batched);
      rpcSystem->Call<GatewayRPC_IngressCommand>(
          _serverAddress, message,
          CommandDeliveryToMessageSendMode(deliveryMode));
      return CommandAckStatus::Sent; // Optimistically assume it was sent
                                     // successfully
    }

    logger->info("Dispatching command '{}' with send mode {} (batched: {}, "
                 "ack expected)",
                 commandName,
                 boost::describe::enum_to_string(deliveryMode, "<INVALID>"),
                 batched);
    auto result = rpcSystem->Call_R<GatewayRPC_IngressCommand>(
        _serverAddress, message,
        CommandDeliveryToMessageSendMode(deliveryMode));
    std::future_status status = result.wait_for(timeout);
    if (status == std::future_status::timeout)
    {
      logger->warn("Command '{}' timed out after {} milliseconds.", commandName,
                   timeout.count());
      return CommandAckStatus::TimedOut;
    }
    else if (status == std::future_status::ready)
    {
      return result.get()->status;
    }
    assert(false && "Unexpected future status");
    return CommandAckStatus::UnknownError; // Should not reach here
  }

  template <typename CMD>
  requires std::is_base_of_v<ICommand, CMD>
  CommandAckStatus AtlasNetClient_DispatchCommand(
      const CMD& command, CommandDeliveryGuarantee sendMode =
                              CommandDeliveryGuarantee::GatewayConfirmedBatched)
  {
    ByteWriter writer;
    command.Serialize(writer);
    return AtlasNetClient_DispatchCommand(command.GetName(), writer.as_string_view(),
                                   sendMode);
  }

private:
  void OnClientConnectionCompleteNotification(
      const ClientConnectionCompleteData& data)
  {
    lastConnectionCompleteData = data;
    lastConnectionCompleteDataCV.notify_all();
  }
  std::optional<ClientConnectionCompleteData> lastConnectionCompleteData;
  std::mutex lastConnectionCompleteDataMutex;
  std::condition_variable lastConnectionCompleteDataCV;

  Network::SocketAddress _serverAddress;
  std::optional<TaskSystem> taskSystem;
  std::optional<MessageSystem> messageSystem;
  std::optional<RPCSystem> rpcSystem;

  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("AtlasNetClient");
};
} // namespace AtlasNet