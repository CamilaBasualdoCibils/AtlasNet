#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include <boost/describe.hpp>
#include <cstdint>
#include <expected>
#include <span>
#include <type_traits>
#include <variant>
namespace AtlasNet
{
using RPCID = uint32_t;
using RPCCallID = uint64_t;
struct RPCContext
{
  Network::SocketAddress sourceAddress;
};

enum class RPCError
{
  None = 0,
  Timeout = 1,
  UnknownRPC = 2,
  InvalidPayload = 3,
  PermissionDenied = 4,
  RemoteException = 5,
};
BOOST_DESCRIBE_ENUM(RPCError, None, Timeout, UnknownRPC, InvalidPayload,
                    PermissionDenied, RemoteException);
template <typename T> using TRPCResult = std::expected<T, RPCError>;
using RPCResult = TRPCResult<std::vector<uint8_t>>;
struct RPCResultW
{
  RPCResult result;
  template <typename Archive> void serialize(Archive& ar) const
  {
    ar(result);
  }
};
using RPC_BindCallFunction_Raw =
    std::function<RPCResult(const RPCContext&, std::span<const uint8_t>)>;

namespace RPC_Internal
{

struct RPCRequestContext
{
  RPCID rpcId;
  RPCCallID callId;
  bool responseExpected;

  template <typename Archive> void serialize(Archive& ar) const
  {
    ar(rpcId, callId, responseExpected);
  }
};
struct RPCResponseContext
{
  RPCID rpcId;
  RPCCallID callId;
  template <typename Archive> void serialize(Archive& ar) const
  {
    ar(rpcId, callId);
  }
};
/* using RPCRequestMessage = Message<"RPCRequestMessage", RPCRequestContext,
std::vector<uint8_t>>; using RPCResponseMessage = Message<"RPCResponseMessage",
RPCResponseContext, RPCResultW>;
 */
enum class RPCMessageType : uint8_t
{
  Request = 0,
  Response = 1,
};
struct RPCHeader
{
  RPCMessageType type;
  std::variant<RPCRequestContext, RPCResponseContext> context;

  template <typename Archive> void serialize(Archive& ar) const
  {
    ar(type, context);
  }
};
/* ATLASNET_MESSAGE(RPCRequestMessage,
                 ATLASNET_MESSAGE_DATA(RPCRequestContext, context),
                 ATLASNET_MESSAGE_DATA(MessageSendMode, sendMode),
                 ATLASNET_MESSAGE_DATA(std::vector<uint8_t>, payload));
ATLASNET_MESSAGE(RPCResponseMessage,
                 ATLASNET_MESSAGE_DATA(RPCResponseContext, context),
                 ATLASNET_MESSAGE_DATA(RPCResultW, result));
 */
constexpr RPCID HashRPCName(const char* str)
{
  RPCID hash = 2166136261u;
  while (*str)
  {
    hash ^= static_cast<uint8_t>(*str++);
    hash *= 16777619u;
  }
  return hash;
}

} // namespace RPC_Internal
template <typename Archive, typename... Ts>
concept DefaultRPCSerializable =
    requires(Archive& ar, Ts&&... args) { ar(std::forward<Ts>(args)...); };

template <FixedString Name, typename Return, typename... Args> struct RPC
{
  using ReturnType = Return;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr RPCID Id = RPC_Internal::HashRPCName(Name.value);
  static constexpr auto NameString = Name;

  template <typename Archive> static void serialize(Archive& ar, Args&&... args)
  {
    static_assert(
        DefaultRPCSerializable<Archive, Args...>,
        "The arguments of this RPC cannot be serialized using the default "
        "RPC serializer. Define a custom RPC::serialize() for this RPC.");
    ar(std::forward<Args>(args)...);
  }

  template <typename Func>
  static constexpr bool Invocable =
      std::is_void_v<Return> ? std::is_invocable_r_v<void, Func, Args...>
                             : std::is_invocable_r_v<ReturnType, Func, Args...>;

  template <typename Func>
  static constexpr bool ContextInvocable =
      std::is_void_v<Return>
          ? std::is_invocable_r_v<void, Func, const RPCContext&, Args...>
          : std::is_invocable_r_v<ReturnType, Func, const RPCContext&, Args...>;
};
/*if the function takes the args and returns the Return then its valid*/

using TestRPC = RPC<"TestRPC", void, int, float>;
} // namespace AtlasNet

/* #define ATLASNET_RPC_NEW(NameSpace, ServiceName, Return, ...) \
  struct ATLASNET_CAT3(NameSpace, _, ServiceName)                              \
  {                                                                            \
    using ReturnType = Return;                                                 \
    using ArgsTuple = std::tuple<__VA_ARGS__>;                                 \
    static constexpr std::string_view Name = #NameSpace "." #ServiceName;      \
    static constexpr RPCID Id =                                                \
        AtlasNet::RPC_Internal::HashRPCName(Name.data());                      \
    template <typename Archive> void serialize(Archive& ar) const {}           \
  };
 */