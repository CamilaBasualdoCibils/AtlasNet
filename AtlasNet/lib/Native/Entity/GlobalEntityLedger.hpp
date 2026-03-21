#pragma once

#include <span>
#include <string>
#include <string_view>
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

		std::unordered_set<std::string> desiredEntityIDs;
		desiredEntityIDs.reserve(entityIDs.size());
		for (const AtlasEntityID& entityID : entityIDs)
		{
			desiredEntityIDs.insert(UUIDGen::ToString(entityID));
		}

		std::vector<std::string> staleOwnedEntityIDs;
		const std::unordered_map<std::string, std::string> entityOwners =
			InternalDB::Get()->HGetAll(entityID2ShardHashMap);
		for (const auto& [entityIDStr, ownerShardIDStr] : entityOwners)
		{
			if (ownerShardIDStr == shardIDStr)
			{
				if (!desiredEntityIDs.contains(entityIDStr))
				{
					staleOwnedEntityIDs.push_back(entityIDStr);
				}
			}
		}

		if (!staleOwnedEntityIDs.empty())
		{
			std::vector<std::string_view> staleOwnedEntityViews;
			staleOwnedEntityViews.reserve(staleOwnedEntityIDs.size());
			for (const std::string& entityIDStr : staleOwnedEntityIDs)
			{
				staleOwnedEntityViews.push_back(entityIDStr);
			}
			(void)InternalDB::Get()->HDel(entityID2ShardHashMap, staleOwnedEntityViews);
		}

		for (const std::string& entityIDStr : desiredEntityIDs)
		{
			(void)InternalDB::Get()->HSet(entityID2ShardHashMap, entityIDStr, shardIDStr);
		}
	}
	void DeleteEntityRecord(const ShardID& NetID, const AtlasEntityID& EntityID)
	{
		std::optional<std::string> existingOwner =
			InternalDB::Get()->HGet(entityID2ShardHashMap, UUIDGen::ToString(EntityID));

		if (existingOwner.has_value() && existingOwner.value() == UUIDGen::ToString(NetID))
		{
			(void)InternalDB::Get()->HDel(entityID2ShardHashMap, {UUIDGen::ToString(EntityID)});
		}
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
