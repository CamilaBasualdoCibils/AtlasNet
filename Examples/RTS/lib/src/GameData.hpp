#pragma once

#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "PlayerColors.hpp"
struct WorkerData
{
	vec3 Position;
	PlayerTeams team;
	void Serialize(ByteWriter& bw) const
	{
		bw.vec3(Position);
		bw.write_scalar<PlayerTeams>(team);
	}
	void Deserialize(ByteReader& br)
	{
		Position = br.vec3();
		team = br.read_scalar<PlayerTeams>();
	}
};