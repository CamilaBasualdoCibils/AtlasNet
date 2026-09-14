#pragma once

#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "boost/container/static_vector.hpp"

#include <variant>
namespace AtlasNet
{
enum class HandshakeRole : uint8_t
{
  eClient = 0,
  eServer = 1,
  eInvalid = 2
};
struct HandshakeServerRequestData
{
  AtlasNetNodeID serviceID;
  AtlasNetNodeType serviceType = AtlasNetNodeType::Invalid;
};
struct HandshakeClientRequestData
{
  boost::container::static_vector<uint8_t, 20> payload;
};
struct HandshakeIdentity
{
  HandshakeRole role = HandshakeRole::eInvalid;
  std::variant<HandshakeServerRequestData, HandshakeClientRequestData> data;

  void Serialize(ByteWriter& writer) const
  {
    assert((role == HandshakeRole::eServer &&
            std::holds_alternative<HandshakeServerRequestData>(data)) ||
           (role == HandshakeRole::eClient &&
            std::holds_alternative<HandshakeClientRequestData>(data)));
    writer.u8(static_cast<uint8_t>(role));
    if (std::holds_alternative<HandshakeServerRequestData>(data))
    {
      const auto& serverData = std::get<HandshakeServerRequestData>(data);
      writer(serverData.serviceID);
      writer.u8(static_cast<uint8_t>(serverData.serviceType));
    }
    else if (std::holds_alternative<HandshakeClientRequestData>(data))
    {
      const auto& clientData = std::get<HandshakeClientRequestData>(data);
      writer.u16(static_cast<uint16_t>(clientData.payload.size()));
      writer.blob(
          std::span(clientData.payload.data(), clientData.payload.size()));
    }
  }
  void Deserialize(ByteReader& reader)
  {
    uint8_t roleByte;
    reader.u8(roleByte);
    role = static_cast<HandshakeRole>(roleByte);
    if (role == HandshakeRole::eServer)
    {
      HandshakeServerRequestData serverData;
      reader(serverData.serviceID);
      uint8_t serviceTypeByte;
      reader.u8(serviceTypeByte);
      serverData.serviceType = static_cast<AtlasNetNodeType>(serviceTypeByte);
      data = serverData;
    }
    else if (role == HandshakeRole::eClient)
    {
      HandshakeClientRequestData clientData;
      uint16_t payloadSize;
      reader.u16(payloadSize);
      clientData.payload.resize(payloadSize);
      std::span<const uint8_t> payloadSpan;
      reader.blob(payloadSpan);
      clientData.payload.clear();
      std::copy(payloadSpan.begin(), payloadSpan.end(),
                clientData.payload.begin());
      data = clientData;
    }
  }
};
 enum class HandshakeResponseCode : uint8_t
{
  eAccept = 0,
  eReject = 1
}; 
struct HandshakeResponsePacket
{
bool accepted;
std::string rejectReason;

  void Serialize(ByteWriter& writer) const
  {
    writer.u8(static_cast<uint8_t>(accepted ? 0 : 1));
    if (!accepted)
    {
      writer.str(rejectReason);
    }
  }
  void Deserialize(ByteReader& reader)
  {
    uint8_t codeByte;
    reader.u8(codeByte);
    accepted = (codeByte == 0);
    if (!accepted)
    {
      reader.str(rejectReason);
    }
  }
};


} // namespace AtlasNet