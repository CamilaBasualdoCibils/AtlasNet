#pragma once

#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include <boost/describe.hpp>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <system_error>
namespace AtlasNet
{

enum class AtlasNetNodeType : uint8_t
{
  Controller = 0,
  Shard = 1,
  Gateway = 2,
  Cartograph = 3,
  Invalid = 4
};
BOOST_DESCRIBE_ENUM(AtlasNetNodeType, Controller, Shard, Gateway, Cartograph,
                    Invalid);


struct GatewayNodeInfo
{
  AtlasNetGatewayID id;
  void Serialize(ByteWriter& bw) const
  {
    bw(id);
  }
  void Deserialize(ByteReader& br)
  {
    br(id);
  }
  void to_json(_Json& j) const
  {
    j = _Json{{"id", id.to_string()}};
  }
};
struct ShardNodeInfo
{
  AtlasNetShardID id;

  void Serialize(ByteWriter& bw) const
  {
    bw(id);
  }
  void Deserialize(ByteReader& br)
  {
    br(id);
  }
  void to_json(_Json& j) const
  {
    j = _Json{{"id", id.to_string()}};
  }
};
struct ControllerNodeInfo
{
  AtlasNetControllerID id;
  void Serialize(ByteWriter& bw) const
  {
    bw(id);
  }
  void Deserialize(ByteReader& br)
  {
    br(id);
  }
  void to_json(_Json& j) const
  {
    j = _Json{{"id", id.to_string()}};
  }
};
struct CartographBackendInfo
{

  void Serialize(ByteWriter& bw) const {}
  void Deserialize(ByteReader& br)
  {
    // Deserialize any relevant information for the CartographBackendInfo
  }
  void to_json(_Json& j) const
  {
    j = _Json{};
  }
};
struct NodeInfo
{
  AtlasNetNodeID id;
  Network::SocketAddress address;
  AtlasNetNodeType containerType;
  std::optional<std::variant<GatewayNodeInfo, ShardNodeInfo, ControllerNodeInfo,
                             CartographBackendInfo>>
      specificInfo;
  void Serialize(ByteWriter& bw) const
  {
    bw(id);
    bw(address);
    bw(containerType);
    // Serialize the specificInfo variant
    if (specificInfo.has_value())
    {
      std::visit([&bw](auto&& arg) { arg.Serialize(bw); }, *specificInfo);
    }
  }
  void Deserialize(ByteReader& br)
  {
    br(id);
    br(address);
    br(containerType);
    // Deserialize the specificInfo variant
    switch (containerType)
    {
    case AtlasNetNodeType::Gateway:
      specificInfo = GatewayNodeInfo{};
      break;
    case AtlasNetNodeType::Shard:
      specificInfo = ShardNodeInfo{};
      break;
    case AtlasNetNodeType::Controller:
      specificInfo = ControllerNodeInfo{};
      break;
    case AtlasNetNodeType::Cartograph:
      specificInfo = CartographBackendInfo{};
      break;
    default:
      throw std::runtime_error("Unknown node type");
    }

    std::visit([&br](auto&& arg) { arg.Deserialize(br); }, *specificInfo);
  }
  void to_json(_Json& j) const
  {
    j = _Json{{"id", id.to_string()},
              {"address", address.to_string()},
              {"containerType",
               boost::describe::enum_to_string(containerType, "UNKNOWN")}};
    if (specificInfo.has_value())
    {
      std::visit(
          [&j](auto&& arg)
          {
            _Json specificJson;
            arg.to_json(specificJson);
            j["specificInfo"] = specificJson;
          },
          *specificInfo);
    }
  }
};
}; // namespace AtlasNet
namespace std
{
template <typename underlying_type, typename tag>
struct hash<AtlasNet::StrongTypedef<underlying_type, tag>>
{
  size_t operator()(
      const AtlasNet::StrongTypedef<underlying_type, tag>& value) const noexcept
  {
    return std::hash<underlying_type>{}(value.value);
  }
};

 template <typename Underlying, typename Tag>
    struct formatter<AtlasNet::StrongTypedef<Underlying, Tag>>
        : formatter<Underlying>
    {
        template <typename FormatContext>
        auto format(const AtlasNet::StrongTypedef<Underlying, Tag>& value,
                    FormatContext& ctx) const
        {
            return formatter<Underlying>::format(value.value, ctx);
        }
    };
} // namespace std