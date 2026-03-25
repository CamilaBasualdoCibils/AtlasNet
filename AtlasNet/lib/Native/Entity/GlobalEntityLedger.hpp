#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"
class GlobalEntityLedger : public Singleton<GlobalEntityLedger>
{
	constexpr static const char* entityID2ShardHashMap = "Entity:EntityOwner";

   public:
	GlobalEntityLedger() = default;
	void DeclareEntityRecord(const ShardID& NetID, const AtlasEntityID& EntityID)
	{
		(void)InternalDB::Get()->HSet(entityID2ShardHashMap, UUIDGen::ToString(EntityID),
									  UUIDGen::ToString(NetID));
	}
	void ReplaceShardEntityRecords(const ShardID& NetID, std::span<const AtlasEntityID> entityIDs)
	{
		const std::string shardIDStr = UUIDGen::ToString(NetID);
		// Refresh known ownership for entities we currently hold, but do not delete older rows.
		// Entity:EntityOwner is treated as the durable session authority ledger.
		for (const AtlasEntityID& entityID : entityIDs)
		{
			(void)InternalDB::Get()->HSet(
				entityID2ShardHashMap, UUIDGen::ToString(entityID), shardIDStr);
		}
	}
	void DeleteEntityRecord(const ShardID& NetID, const AtlasEntityID& EntityID)
	{
		(void)NetID;
		(void)EntityID;
		// Entity:EntityOwner is the durable authority ledger for the session.
		// Ownership changes should overwrite the owner entry, not remove it.
	}
	// using back inserter
	void GetAllEntitiesInShard(const ShardID& NetID,
							   std::back_insert_iterator<std::vector<AtlasEntityHandle>> inserter)
	{
		const std::string shardIDStr = UUIDGen::ToString(NetID);
		const std::unordered_map<std::string, std::string> entityOwners =
			InternalDB::Get()->HGetAll(entityID2ShardHashMap);
		for (const auto& [entityIDStr, ownerShardIDStr] : entityOwners)
		{
			if (ownerShardIDStr != shardIDStr)
			{
				continue;
			}
			AtlasEntityHandle handle(UUIDGen::FromString(entityIDStr));
			inserter = std::move(handle);
		}
	}

	void GetAllEntities(std::back_insert_iterator<std::vector<AtlasEntityHandle>> inserter)
	{
		std::vector<std::string> entityIDStrs = InternalDB::Get()->HKeys(entityID2ShardHashMap);
		for (const auto& idStr : entityIDStrs)
		{
			AtlasEntityHandle handle(UUIDGen::FromString(idStr));
			inserter = std::move(handle);
			/* logger.DebugFormatted("Found EntityID in Global Ledger: {}", idStr); */
		}
	}
	void GetAllEntityOwners(std::unordered_map<AtlasEntityID, ShardID>& out)
	{
		out.clear();
		const std::unordered_map<std::string, std::string> entityOwners =
			InternalDB::Get()->HGetAll(entityID2ShardHashMap);
		out.reserve(entityOwners.size());
		for (const auto& [entityIDStr, ownerShardIDStr] : entityOwners)
		{
			out.emplace(UUIDGen::FromString(entityIDStr), UUIDGen::FromString(ownerShardIDStr));
		}
	}

	std::optional<ShardID> GetEntityOwnerShard(const AtlasEntityID& EntityID)
	{
		std::optional<std::string> ownerStr =
			InternalDB::Get()->HGet(entityID2ShardHashMap, UUIDGen::ToString(EntityID));
		if (ownerStr.has_value())
		{
			return UUIDGen::FromString(ownerStr.value());
		}
		return std::nullopt;
	}

   private:
};
