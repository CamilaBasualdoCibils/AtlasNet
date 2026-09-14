#pragma once

#include "AtlasNet/Core/Network/Address/SocketAddress.hpp"
#include <boost/describe.hpp>
#include <cstdint>
#include <expected>
#include <iostream>
#include <limits>
#include <vector>
namespace AtlasNet::Network::RPC
{
using RPCMethodID = uint64_t;
using RPCCallID = uint64_t;
constexpr RPCMethodID HashRPC(std::string_view qualifiedName)
{
  constexpr std::uint64_t OFFSET = 14695981039346656037ULL;
  constexpr std::uint64_t PRIME = 1099511628211ULL;

  std::uint64_t hash = OFFSET;
  /*  if (!std::is_constant_evaluated())
   {
     std::cerr << "HashRPC size=" << qualifiedName.size() << "\nbytes:";

     for (unsigned char c : qualifiedName)
       std::cerr << ' ' << std::hex << static_cast<unsigned>(c);

     std::cerr << std::dec << '\n';
   } */
  for (char c : qualifiedName)
  {
    if (c == '.')
    {
      // Explicit namespace boundary
      hash ^= 0xFF;
      hash *= PRIME;
      continue;
    }

    hash ^= static_cast<std::uint8_t>(c);
    hash *= PRIME;
  }
  /*    if (!std::is_constant_evaluated())
          std::cerr << "hash=" << hash << '\n';
    std::cerr << "HashRPC(" << qualifiedName << ") = " << hash << std::endl; */
  return hash;
}
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
using RPCResult = TRPCResult<std::vector<std::byte>>;

enum class RPCMessageType : uint8_t
{
  Call = 0,    // Invoke; no return value requested
  Request = 1, // Invoke; response requested
  Response = 2 // Return value from a previous request
};
struct RPCHeader
{

  RPCMethodID methodId;
  RPCCallID callId;
  RPCMessageType type;
  uint16_t payloadSize;
  constexpr static uint64_t MaxPayloadSize =
      std::numeric_limits<decltype(payloadSize)>::max();
  template <typename Archive> void serialize(Archive& ar)
  {
    ar(methodId, callId, type, payloadSize);
  }
};
template <typename Return, typename Func, typename Context, typename Tuple>
struct RPCInvocableHelper;

template <typename Return, typename Func, typename Context, typename... Args>
struct RPCInvocableHelper<Return, Func, Context, std::tuple<Args...>>
{
  static constexpr bool value =
      std::is_invocable_r_v<Return, Func, Context, const Args&...>;
};

template <typename RPC, typename Func, typename Context>
concept RPCInvocable =
    RPCInvocableHelper<typename RPC::ReturnType, Func, Context,
                       typename RPC::ArgsTuple>::value;
} // namespace AtlasNet::Network::RPC