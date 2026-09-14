#pragma once
#include "AtlasNet/Core/Network/RPC/RPCCommons.hpp"
#include "AtlasNet/Core/Types/FixedString.hpp"
namespace AtlasNet::Network::RPC
{
template <typename Archive, typename... Ts>
concept DefaultRPCSerializable =
    requires(Archive& ar, Ts&&... args) { ar(std::forward<Ts>(args)...); };

template <FixedString Name, typename Return, typename... Args> struct RPCMethod
{
  using ReturnType = Return;
  using ArgsTuple = std::tuple<Args...>;
  static constexpr RPCMethodID Id = HashRPC(Name.value);
  static constexpr auto NameString = Name;
  template <typename Archive> static void serialize(Archive& ar, Args&&... args)
  {
    static_assert(
        DefaultRPCSerializable<Archive, Args...>,
        "The arguments of this RPC cannot be serialized using the default "
        "RPC serializer. Define a custom RPC::serialize() for this RPC.");
    ar(std::forward<Args>(args)...);
  }
};
} // namespace AtlasNet::Network::RPC