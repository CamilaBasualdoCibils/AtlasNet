#pragma once

#include <sw/redis++/redis.h>

#include <boost/describe/enum_to_string.hpp>

#include "Entity/Transfer/TransferData.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"
class TransferManifest : public Singleton<TransferManifest>
{
	const std::string TransferManifestTableName = "Transfer::TransferManifest";

	void EnsureJsonTable()
	{
		(void)InternalDB::Get()->WithSync(
			[&](auto& r)
			{
				std::array<std::string, 5> cmd = {
					"JSON.SET", TransferManifestTableName, ".",
					R"({"EntityTransfers": {}, "ClientTransfers": {}})", "NX"};

				return r.command(cmd.begin(), cmd.end());
			});
	}

   public:
	void PushTransferInfo(const EntityTransferData& data)
	{
		EnsureJsonTable();

		// Precompute the path OUTSIDE the lambda to avoid format inside template
		const std::string path = ".EntityTransfers." + UUIDGen::ToString(data.ID);

		Json json;
		NetworkIdentity fromID = NetworkCredentials::Get().GetID();
		json["From"] = fromID.ToString();
		ByteWriter FromWrite;
		fromID.Serialize(FromWrite);
		json["From(64)"] = FromWrite.as_string_base_64();

		json["To"] = data.receiver.ToString();
		ByteWriter ToWrite;
		data.receiver.Serialize(ToWrite);
		json["To(64)"] = ToWrite.as_string_base_64();

		json["stage"] = boost::describe::enum_to_string(data.stage, "INVALID");

		for (const auto& EntityId : data.entityIDs)
		{
			json["EntityIDs"].push_back(UUIDGen::ToString(EntityId));
		}
		(void)InternalDB::Get()->WithSync(
			[&](auto& r)
			{
				std::array<std::string, 4> cmd = {"JSON.SET", TransferManifestTableName, path,
												  json.dump()};

				return r.command(cmd.begin(), cmd.end());
			});
	}
};

/*
void HeuristicManifest::Internal_SetActiveHeuristicType(IHeuristic::Type type)
{
	Internal_EnsureJsonTable();
	logger.DebugFormatted("Set HeuristicType {}", IHeuristic::TypeToString(type));
	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 4> set_type = {
				"JSON.SET", JSONDataTable,
				"." + JSONHeuristicTypeEntry,						   // ".HeuristicType"
				std::format("\"{}\"", IHeuristic::TypeToString(type))  // JSON string value
			};

			r.command(set_type.begin(), set_type.end());
		});
}*/