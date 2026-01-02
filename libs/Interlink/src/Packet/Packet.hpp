#pragma once

/// @brief Packet Types Internal to InterLink
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
enum class PacketType
{
	eInvalid = 0,
	eRelay = 1,
	eCommand = 2,
};
BOOST_DESCRIBE_ENUM(PacketType, eInvalid, eRelay)

class IPacket
{
	PacketType packet_type = PacketType::eInvalid;

   public:
	IPacket(PacketType t) : packet_type(t) {}
	virtual ~IPacket() = default;
	IPacket& SetPacketType(PacketType t)
	{
		packet_type = t;
		return *this;
	}
	PacketType GetPacketType(PacketType t) { return packet_type; }
	IPacket& Serialize(ByteWriter& bw) const;
	IPacket& Deserialize(ByteReader& br);

   protected:
	virtual void SerializeData(ByteWriter& bw) const = 0;
	virtual void DeserializeData(ByteReader& br) = 0;
};
