#pragma once

#include <cstdint>

#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"

class HandoffPingPacket
	: public TPacket<HandoffPingPacket, "HandoffPingPacket">
{
  public:
	NetworkIdentity sender;
	uint64_t sentAtMs = 0;

  private:
	void SerializeData(ByteWriter& writer) const override
	{
		sender.Serialize(writer);
		writer.write_scalar(sentAtMs);
	}

	void DeserializeData(ByteReader& reader) override
	{
		sender.Deserialize(reader);
		sentAtMs = reader.read_scalar<uint64_t>();
	}

	[[nodiscard]] bool ValidateData() const override
	{
		return sender.Type != NetworkIdentityType::eInvalid;
	}
};

ATLASNET_REGISTER_PACKET(HandoffPingPacket, "HandoffPingPacket");
