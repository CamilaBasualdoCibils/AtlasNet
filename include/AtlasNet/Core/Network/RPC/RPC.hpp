#pragma once
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/RPC/RPCCommons.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <atomic>
#include <future>
#include <memory>
#include <shared_mutex>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <string_view>
#include <type_traits>
#include <unordered_map>
namespace AtlasNet::Network::RPC
{
template <typename Target, typename Caller> class RPC
{
public:
  RPC(std::string_view Name)
      : logger(spdlog::stdout_color_mt(std::string(Name)))
  {
  }
  struct CallContext
  {
    Target intendedTarget;
    Caller caller;
  };
  using BindCallFunction_Raw = std::move_only_function<std::vector<std::byte>(
      const CallContext&, std::span<const std::byte>)>;
  using TargetType = Target;

  void Bind(std::string_view methodName, BindCallFunction_Raw func)
  {
    RPCMethodID methodId = HashRPC(methodName);
    logger->info("Binding RPC {} -> {}", methodName, methodId);
    std::unique_lock lock(mutex);
    bindings_[methodId] = std::move(func);
  }
  template <typename rpc, typename Func>
    requires(RPCInvocable<rpc, Func, const CallContext&>)
  void Bind(Func&& func)
  {
    Bind(rpc::NameString.value,
         [func = std::move(func)](
             const CallContext& ctx,
             std::span<const std::byte> packetdata) -> std::vector<std::byte>
         {
           NetBinaryReader reader(packetdata);
           typename rpc::ArgsTuple args;
           reader(args);
           if constexpr (!std::is_void_v<typename rpc::ReturnType>)
           {
             auto res = std::apply(
                 [&](auto&&... unpacked)
                 {
                   return func(ctx,
                               std::forward<decltype(unpacked)>(unpacked)...);
                 },
                 args);
             NetBinaryWriter writer;
             writer(res);
             return writer.Release();
           }
           else
           {
             std::apply(
                 [&](auto&&... unpacked)
                 { func(ctx, std::forward<decltype(unpacked)>(unpacked)...); },
                 args);
             return {};
           }
         });
  }
  void Call(const Target& target, std::string_view methodName,
            std::span<const std::byte> payload)
  {
    logger->debug("Calling RPC {} on target {}[ResponseExpected={}]", methodName,
                 target.to_string(), false);
    RPCMethodID methodId = HashRPC(methodName);

    RPCCallID callId = nextCallID++;
    _SendPacket(target, methodId, callId, RPCMessageType::Call, payload);
  }
  [[nodiscard]] std::future<RPCResult>
  Call_R(const Target& target, std::string_view methodName,
         std::span<const std::byte> payload)
  {
    logger->debug("Calling RPC {} on target {}[ResponseExpected={}]", methodName,
                 target.to_string(), true);
    RPCMethodID methodId = HashRPC(methodName);

    RPCCallID callId = nextCallID++;
    _SendPacket(target, methodId, callId, RPCMessageType::Request, payload);
    std::unique_lock lock(mutex);
    std::promise<RPCResult> promise;
    std::future<RPCResult> future = promise.get_future();
    pendingCalls[callId] = pendingCall{
        .timestamp = std::chrono::steady_clock::now(),
        .onComplete = [promise = std::move(promise)](
                          const pendingCall&, const RPCResult& result) mutable
        { promise.set_value(result); },
    };
    return future;
  }

  template <typename rpc, typename Args = typename rpc::ArgsTuple>
  [[nodiscard]] std::conditional_t<
      std::is_void_v<typename rpc::ReturnType>, void,
      std::future<TRPCResult<typename rpc::ReturnType>>>
  Call(const Target& target, const rpc::ArgsTuple& args)
  {
    NetBinaryWriter writer;
    writer(args);
    if constexpr (std::is_void_v<typename rpc::ReturnType>)
    {

      Call(target, rpc::NameString.value, writer.GetBytes());
    }
    else
    {
      logger->debug("Calling RPC {} on target {}[ResponseExpected={}]",
                   (std::string_view)rpc::NameString, target.to_string(), true);
      RPCMethodID methodId = HashRPC(rpc::NameString);

      RPCCallID callId = nextCallID++;
      _SendPacket(target, methodId, callId, RPCMessageType::Request,
                  writer.GetBytes());
      std::promise<TRPCResult<typename rpc::ReturnType>> promise;
      std::future<TRPCResult<typename rpc::ReturnType>> future =
          promise.get_future();
      std::unique_lock lock(mutex);
      pendingCalls[callId] = pendingCall{
          .timestamp = std::chrono::steady_clock::now(),
          .onComplete =
              [promise = std::move(promise)](const pendingCall&,
                                             const RPCResult& result) mutable
          {
            NetBinaryReader reader(result.value());
            typename rpc::ReturnType returnValue;
            reader(returnValue);
            promise.set_value(returnValue);
          },
      };
      return future;
    }
  }
  void Poll(PollType pollType)
  {
    _ImplPoll(pollType);
  }

protected:
  void _SendPacket(const Target& target, RPCMethodID methodId, RPCCallID callId,
                   RPCMessageType messageType,
                   std::span<const std::byte> payload)
  {
    RPCHeader header{
        .methodId = methodId,
        .callId = callId,
        .type = messageType,
        .payloadSize = static_cast<uint16_t>(payload.size()),
    };
    NetBinaryWriter writer;
    writer(header);
    writer->adapter().writeBuffer<sizeof(uint8_t)>(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    _ImplSendPacket(target, writer.GetBytes());
  }
  virtual void _ImplSendPacket(const Target& target,
                               std::span<const std::byte> packetdata) = 0;
  virtual void _ImplPoll(PollType pollType) = 0;
  void HandleIncomingPacket(const Caller& caller, const Target& intendedTarget,
                            std::span<const std::byte> data)
  {
    RPCHeader header;
    NetBinaryReader reader(data);
    reader(header);
    std::span<const std::byte> payload(data.data() +
                                           reader->adapter().currentReadPos(),
                                       data.data() + data.size());
    switch (header.type)
    {
    case RPCMessageType::Call:
    case RPCMessageType::Request:
    {
      std::shared_lock<std::shared_mutex> lock(mutex);
      auto it = bindings_.find(header.methodId);
      if (it == bindings_.end())
      {
        logger->error("Received Call from {} to unknown method ID {}",
                      CallerToString(caller), header.methodId);
        return;
      }
      RPCResult result;
      CallContext callContext{.intendedTarget = intendedTarget,
                              .caller = caller};
                              logger->debug("Calling RPC method ID {} from caller {} with intended target {}",
                      header.methodId, CallerToString(caller), TargetToString(intendedTarget));
      result = it->second(callContext, payload);
      NetBinaryWriter resultWriter;
      resultWriter(result);
      if (header.type == RPCMessageType::Request)
      {
        _SendPacket(caller, header.methodId, header.callId,
                    RPCMessageType::Response, resultWriter.GetBytes());
      }
    }
    break;
    case RPCMessageType::Response:
    {
      std::shared_lock<std::shared_mutex> lock(mutex);
      auto it = pendingCalls.find(header.callId);
      if (it == pendingCalls.end())
      {
        logger->error("Received Response for unknown call ID {}",
                      header.callId);
        return;
      }
      pendingCall pendingCall = std::move(it->second);
      pendingCalls.erase(it);
      lock.unlock();
      RPCResult result;
      NetBinaryReader reader(payload);
      reader(result);
      pendingCall.onComplete(pendingCall, result);
    }
    break;
    default:
      // Unknown message type
      break;
    }
  }
  virtual std::string CallerToString(const Caller& caller) const = 0;
  virtual std::string TargetToString(const Target& target) const = 0;
  std::shared_mutex mutex;
  std::unordered_map<RPCMethodID, BindCallFunction_Raw> bindings_;
  std::atomic<RPCCallID> nextCallID{0};
  std::shared_ptr<spdlog::logger> logger;
  struct pendingCall
  {
    // std::promise<RPCResult> promise;
    std::chrono::steady_clock::time_point timestamp;
    std::move_only_function<void(const pendingCall&, const RPCResult&)>
        onComplete;
  };
  std::unordered_map<RPCCallID, pendingCall> pendingCalls;
};
} // namespace AtlasNet::Network::RPC