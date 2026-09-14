#pragma once

#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/core/entity/Entity.hpp"

namespace AtlasNet
{
struct LoginData
{
  Network::SocketAddress address;
  AtlasNetGatewayID managingGateway;
  AtlasNetClientID clientID;
  std::optional<AtlasNetEntityID> entityID;

  void Serialize(ByteWriter& writer) const
  {
    address.Serialize(writer);
    writer(managingGateway); 
    clientID.Serialize(writer);
    if (entityID.has_value())
    {
      writer.u8(true);
      entityID.value().Serialize(writer);
    }
    else
    {
      writer.u8(false);
    }
  }
  void Deserialize(ByteReader& reader)
  {
    address.Deserialize(reader);
    reader(managingGateway); 
    clientID.Deserialize(reader);
    uint8_t hasEntityID_v;
    reader.u8(hasEntityID_v);
    bool hasEntityID = hasEntityID_v != 0;
    if (hasEntityID)
    {
      AtlasNetEntityID id;
      id.Deserialize(reader);
      entityID = id;
    }
    else
    {
      entityID = std::nullopt;
    }
  }

  void to_json(_Json& j) const
  {
    j = _Json{
        {"address", address.to_string()},
        {"managingGateway", managingGateway.to_string()},
        {"clientID", clientID.to_string()},
        {"entityID",
         entityID.has_value() ? entityID.value().to_string() :""},
    };
  }
};
}; // namespace AtlasNet