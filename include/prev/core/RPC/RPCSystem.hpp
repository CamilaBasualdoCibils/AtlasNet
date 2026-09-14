#pragma once

#include "AtlasNet/Core/Network/Intent/ClusterIntentChannel.hpp"
#include "RPCConcepts.hpp"
#include "spdlog/sinks/stdout_color_sinks-inl.h"
#include <future>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
namespace AtlasNet
{

class RPCSystem
{

public:
  struct Config
  {
    std::shared_ptr<Network::Intent::ClusterIntentChannel> intentChannel;
    std::chrono::milliseconds timeout = std::chrono::seconds(5);
  };
  RPCSystem(const Config& config);
  void Bind(std::string_view name, RPC_BindCallFunction_Raw func);
  void Bind(RPCID id, RPC_BindCallFunction_Raw func);

  template <typename rpc, typename Func>
    requires(rpc::template Invocable<Func> ||
             rpc::template ContextInvocable<Func>)
  void Bind(Func&& func)
  {
    logger->info("Binding RPC {} -> {}", rpc::NameString.value, rpc::Id);
    RPC_BindCallFunction_Raw rawFunc =
        [func = std::move(func)](const RPCContext& context,
                                 std::span<const uint8_t> payload) -> RPCResult
    {
      // deserialize the payload
      NetBinaryReader deserializer(payload);
      using Return = typename rpc::ReturnType;
      using ArgsTuple = typename rpc::ArgsTuple;
      ArgsTuple args;
      deserializer(args);
      RPCResult result;
      if constexpr (std::is_void_v<Return>)
      {
        std::apply(
            [&](auto&... xs)
            {
              if constexpr (rpc::template ContextInvocable<Func>)
                std::invoke(func, context, xs...);
              else
                std::invoke(func, xs...);
            },
            args);

        return result;
      }
      else
      {
        Return ret = std::apply(
            [&](auto&... xs) -> Return
            {
              if constexpr (rpc::template ContextInvocable<Func>)
                return std::invoke(func, context, xs...);
              else
                return std::invoke(func, xs...);
            },
            args);

        NetBinaryWriter writer;

        writer(ret);
        auto bytes = writer.GetBytes();
        // result is a std::expected<vector<uint8_t>, RPCError>
        result = std::vector<uint8_t>(bytes.begin(), bytes.end());

        return result;
      }
    };
    std::unique_lock lock(mutex);
    bindings[rpc::Id] = std::move(rawFunc);
  }

  void Call(Network::SocketAddress target, std::string_view methodName,
            std::span<const uint8_t> payload,MessageSendMode mode = MessageSendMode::eReliableBatched);
  void Call(Network::SocketAddress target, RPCID methodId,
            std::span<const uint8_t> payload,MessageSendMode mode = MessageSendMode::eReliableBatched);

  [[nodiscard]] std::future<RPCResult> Call_R(Network::SocketAddress target,
                                              std::string_view methodName,
                                              std::span<const uint8_t> payload, MessageSendMode mode = MessageSendMode::eReliableBatched);
  [[nodiscard]] std::future<RPCResult> Call_R(Network::SocketAddress target,
                                              RPCID methodId,
                                              std::span<const uint8_t> payload, MessageSendMode mode = MessageSendMode::eReliableBatched);

  template <typename rpc, typename Args = typename rpc::ArgsTuple>
  void Call(Network::SocketAddress target, const Args& args, MessageSendMode mode = MessageSendMode::eReliableBatched)
  {
    logger->info("Calling RPC {} -> {} on target {}", rpc::NameString.value, rpc::Id, target.to_string());
    std::vector<uint8_t> payload;
    NetBinaryWriter serializer;
    serializer(args);
    auto bytes = serializer.GetBytes();
    payload.insert(payload.end(), bytes.begin(), bytes.end());
    Call(target, rpc::Id, payload, mode);
  }
  template <typename rpc, typename Args = typename rpc::ArgsTuple>
    requires(!std::is_void<typename rpc::ReturnType>::value)
  [[nodiscard]] std::future<TRPCResult<typename rpc::ReturnType>>
  Call_R(Network::SocketAddress target, const Args& args, MessageSendMode mode = MessageSendMode::eReliableBatched)
  {
    logger->info("Calling RPC {} -> {} on target {}, response expected", rpc::NameString.value, rpc::Id, target.to_string());
    static_assert(!std::is_void<typename rpc::ReturnType>::value,
                  "Use the void overload of Call for RPCs with no return value");
    RPCCallID callId = nextCallID++;
    std::unique_lock lock(mutex);
    pendingCall newCall;
    newCall.timestamp = std::chrono::steady_clock::now();
    std::promise<TRPCResult<typename rpc::ReturnType>> typedPromise;
    std::future<TRPCResult<typename rpc::ReturnType>> future =
        typedPromise.get_future();
    newCall.onComplete =
        [typedPromise = std::move(typedPromise)](
            const pendingCall& call, const RPCResult& result) mutable
    {
      if (result.has_value())
      {
        NetBinaryReader deserializer(result.value());
        typename rpc::ReturnType returnValue;
        deserializer(returnValue);
        typedPromise.set_value(
            TRPCResult<typename rpc::ReturnType>{std::move(returnValue)});
      }
      else
      {
        typedPromise.set_value(std::unexpected(RPCError::RemoteException));
      }
    };
    pendingCalls.emplace(callId, std::move(newCall));
    lock.unlock();
    std::vector<uint8_t> payload;
    NetBinaryWriter serializer;
    serializer(args);
    auto bytes = serializer.GetBytes();
    payload.insert(payload.end(), bytes.begin(), bytes.end());

    _SendCallRequest(target,
                     RPC_Internal::RPCRequestContext{
                         .rpcId = rpc::Id,
                         .callId = callId,
                         .responseExpected = true,
                     },
                     payload, mode);
    return future;
  }

private:
  void _SendCallRequest(Network::SocketAddress target,
                        RPC_Internal::RPCRequestContext int_context,
                        std::span<const uint8_t> payload, MessageSendMode mode);
  void _HandleCall(const RPC_Internal::RPCRequestMessage& request,
                   const Network::SocketAddress& sender);
  void _HandleResponse(const RPC_Internal::RPCResponseMessage& response,
                       const Network::SocketAddress& sender);
  const Config config_;
  std::shared_mutex mutex;
  std::unordered_map<RPCID, RPC_BindCallFunction_Raw> bindings;
  std::atomic<RPCCallID> nextCallID{0};
  struct pendingCall
  {
    // std::promise<RPCResult> promise;
    std::chrono::steady_clock::time_point timestamp;
    std::move_only_function<void(const pendingCall&, const RPCResult&)>
        onComplete;
  };
  std::unordered_map<RPCCallID, pendingCall> pendingCalls;
  std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("RPC");
};
}; // namespace AtlasNet