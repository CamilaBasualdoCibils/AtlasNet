
#include "atlasnet/core/RPC/RPCSystem.hpp"
#include "atlasnet/core/RPC/RPCConcepts.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"

void AtlasNet::RPCSystem::_SendCallRequest(
    Network::SocketAddress target, RPC_Internal::RPCRequestContext int_context,
    std::span<const uint8_t> payload,MessageSendMode mode)
{
  RPC_Internal::RPCRequestMessage message{
      .context = std::move(int_context),
      .sendMode = mode,
      .payload = std::vector<uint8_t>(payload.begin(), payload.end()),
  };
  auto msg = config_.messageSystem->QueueMessage(
      message, target, mode);
  std::future_status status = msg->wait_for(config_.timeout);
  assert(status == std::future_status::ready &&
         "Failed to send RPC call: message send timed out");
  MessageSendResultCode resultCode = msg->get().code;
  assert(resultCode == MessageSendResultCode::eSuccess &&
         "Failed to send RPC call: message send failed");
}
void AtlasNet::RPCSystem::_HandleCall(
    const RPC_Internal::RPCRequestMessage& request, const Network::SocketAddress& sender)
{
  const RPCID rpcId = request.context.rpcId;
  const RPCCallID callId = request.context.callId;
  const bool responseExpected = request.context.responseExpected;

  std::shared_lock lock(mutex);
  auto it = bindings.find(rpcId);
  if (it == bindings.end())
  {

    lock.unlock();
    logger->error("Non bound RPC called: {}, from address: {}", rpcId,
                  sender.to_string());
    if (responseExpected)
    {
      RPC_Internal::RPCResponseMessage response{
          .context = RPC_Internal::RPCResponseContext{.rpcId = rpcId,
                                                      .callId = callId},
          .result = RPCResultW{std::unexpected(RPCError::UnknownRPC)}};
      auto msg = config_.messageSystem->QueueMessage(
          response, sender, request.sendMode);
      auto status = msg->wait_for(config_.timeout);
      assert(status == std::future_status::ready &&
             "Failed to send RPC response: message send timed out");
      MessageSendResultCode resultCode = msg->get().code;
      assert(resultCode == MessageSendResultCode::eSuccess &&
             "Failed to send RPC response: message send failed");
    }
  }
  else
  {
    logger->info("Received RPC call for RPC {} from {}", rpcId, sender.to_string());
    RPC_BindCallFunction_Raw func = it->second;
    lock.unlock();
    RPCResult result = func(RPCContext{.sourceAddress = sender}, request.payload);

    if (responseExpected)
    {
      RPC_Internal::RPCResponseMessage response{
          .context = RPC_Internal::RPCResponseContext{.rpcId = rpcId,
                                                      .callId = callId},
          .result = RPCResultW{std::move(result)}};
      auto msg = config_.messageSystem->QueueMessage(
          response, sender, request.sendMode);
      auto status = msg->wait_for(config_.timeout);
      logger->info("Sending RPC response for RPC {} to {}, result: {}", rpcId,
                   sender.to_string(),
                   result.has_value() ? "success" : "timeout/error");
      assert(status == std::future_status::ready &&
             "Failed to send RPC response: message send timed out");
      MessageSendResultCode resultCode = msg->get().code;
      assert(resultCode == MessageSendResultCode::eSuccess &&
             "Failed to send RPC response: message send failed");
    }
  }
}

void AtlasNet::RPCSystem::_HandleResponse(
    const RPC_Internal::RPCResponseMessage& response,
    const Network::SocketAddress& sender)
{
  logger->info("Received RPC response for RPC {} from {}, {}",
               response.context.rpcId, sender.to_string(),
               response.result.result.has_value()
                   ? "success"
                   : boost::describe::enum_to_string(
                         response.result.result.error(), "<UNKNOWN ERROR>"));
  const RPCID rpcId = response.context.rpcId;
  const RPCCallID callId = response.context.callId;
  std::unique_lock lock(mutex);
  auto it = pendingCalls.find(callId);
  if (it != pendingCalls.end())
  {
    it->second.onComplete(it->second, response.result.result);
    pendingCalls.erase(it);
    lock.unlock();
  }
  else
  {
    logger->error(
        "Received response for unknown RPC ID: {},  call ID: {}, address: {}",
        rpcId, callId, sender.to_string());
  }
}

AtlasNet::RPCSystem::RPCSystem(const Config& config) : config_(config)
{
  assert(config_.messageSystem &&
         "RPCSystem requires a MessageSystem in its config");
  config_.messageSystem->On<RPC_Internal::RPCRequestMessage>(
      [this](const RPC_Internal::RPCRequestMessage& request,
             const Network::SocketAddress& sender) { _HandleCall(request, sender); });
  config_.messageSystem->On<RPC_Internal::RPCResponseMessage>(
      [this](const RPC_Internal::RPCResponseMessage& response,
             const Network::SocketAddress& sender)
      { _HandleResponse(response, sender); });
}
void AtlasNet::RPCSystem::Bind(std::string_view name,
                               RPC_BindCallFunction_Raw func)
{
  const RPCID id = RPC_Internal::HashRPCName(name.data());
  Bind(id, std::move(func));
}
void AtlasNet::RPCSystem::Bind(RPCID id, RPC_BindCallFunction_Raw func)
{
  logger->info("Binding RPC {}", id);
  std::unique_lock lock(mutex);
  bindings[id] = std::move(func);
}
void AtlasNet::RPCSystem::Call(Network::SocketAddress target,
                               std::string_view methodName,
                               std::span<const uint8_t> payload, MessageSendMode mode)
{
  const RPCID methodId = RPC_Internal::HashRPCName(methodName.data());
  Call(target, methodId, payload, mode);
}
void AtlasNet::RPCSystem::Call(Network::SocketAddress target, RPCID methodId,
                               std::span<const uint8_t> payload, MessageSendMode mode)
{
  logger->info("Calling RPC {} on target {}", methodId, target.to_string());

  RPCCallID callId = nextCallID++;
  _SendCallRequest(target,
                   RPC_Internal::RPCRequestContext{
                       .rpcId = methodId,
                       .callId = callId,
                       .responseExpected = false,
                   },
                   payload, mode);
}
[[nodiscard]] std::future<AtlasNet::RPCResult>
AtlasNet::RPCSystem::Call_R(AtlasNet::Network::SocketAddress target,
                            std::string_view methodName,
                            std::span<const uint8_t> payload, MessageSendMode mode)
{
  const RPCID methodId = RPC_Internal::HashRPCName(methodName.data());
  return Call_R(target, methodId, payload, mode);
}
[[nodiscard]] std::future<AtlasNet::RPCResult>
AtlasNet::RPCSystem::Call_R(AtlasNet::Network::SocketAddress target, RPCID methodId,
                            std::span<const uint8_t> payload, MessageSendMode mode)
{
  logger->info("Calling RPC {} on target {}, response expected", methodId,
               target.to_string());
  RPCCallID callId = nextCallID++;
  std::unique_lock<std::shared_mutex> lock(mutex);
  pendingCall newCall;
  std::promise<RPCResult> promise;
  std::future<RPCResult> future = promise.get_future();
  newCall.timestamp = std::chrono::steady_clock::now();
  newCall.onComplete =
      [promise = std::move(promise)](const pendingCall& call,
                                     const RPCResult& result) mutable
  { promise.set_value(result); };
  pendingCalls[callId] = std::move(newCall);
  lock.unlock();
  _SendCallRequest(target,
                   RPC_Internal::RPCRequestContext{
                       .rpcId = methodId,
                       .callId = callId,
                       .responseExpected = true,
                   },
                   payload, mode);
  return future;
}