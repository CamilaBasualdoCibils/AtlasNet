#pragma once
#include <steam/steamtypes.h>
#include <boost/container/small_vector.hpp>
#include <cstdint>
#include <span>
#include "Global/AtlasObject.hpp"
#include "Client/Client.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Transform.hpp"
#include "Global/pch.hpp"
using AtlasEntityID = uint64_t;

struct AtlasEntityMinimal: AtlasObject
{

	AtlasEntityID Entity_ID;
	Transform transform;
	bool IsClient;
	ClientID Client_ID;
	void Serialize(ByteWriter& bw) const override
	{
		bw.write_scalar(Entity_ID);
		transform.Serialize(bw);
		bw.u8(IsClient);
		bw.uuid(Client_ID);

	}
	void Deserialize(ByteReader& br) override
	{
		Entity_ID = br.read_scalar<decltype(Entity_ID)>();
		transform.Deserialize(br);
		IsClient = br.u8();
		Client_ID = br.uuid();
	}
};
struct AtlasEntity : AtlasEntityMinimal
{
	
	boost::container::small_vector<uint8,32> Metadata;

	void Serialize(ByteWriter& bw) const override
	{
		AtlasEntityMinimal::Serialize(bw);

		bw.blob(Metadata);
	}
	void Deserialize(ByteReader& br) override
	{
		AtlasEntityMinimal::Deserialize(br);
		const auto metadataBlob = br.blob();
		Metadata.assign(metadataBlob.begin(), metadataBlob.end());

	}
};