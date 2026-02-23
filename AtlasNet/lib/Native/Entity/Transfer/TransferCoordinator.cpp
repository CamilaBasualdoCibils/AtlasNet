#include "TransferCoordinator.hpp"

#include "Entity/Transfer/TransferManifest.hpp"

void TransferCoordinator::ParseEntitiesForTargets()
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	const std::unique_ptr<IHeuristic> heuristic = HeuristicManifest::Get().PullHeuristic();
	std::unordered_map<IBounds::BoundsID, EntityTransferData> NewEntityTransfers;

	while (!EntitiesToParseForReceiver.empty())
	{
		AtlasEntityID entityID = EntitiesToParseForReceiver.top();
		EntitiesToParseForReceiver.pop();

		// If ID already exists in EntitiesInTransfer then entity was already scheduled to be
		// transfered
		if (EntitiesInTransfer.contains(entityID))
			continue;

		if (EntityLedger::Get().IsEntityClient(entityID))
		{
			logger.ErrorFormatted("Client Transfering is not yet implemented, Entity ID {}",
								  UUIDGen::ToString(entityID));
			continue;
		}
		// Get the Bounds ID at the entity position
		const std::optional<IBounds::BoundsID> boundsID = heuristic->QueryPosition(
			EntityLedger::Get().GetEntity(entityID).data.transform.position);

		// if not a bound, aka outside any bound then continue
		if (!boundsID.has_value())
		{
			continue;
		}
		NewEntityTransfers[*boundsID].entityIDs.push_back(entityID);

		// heuristic->QueryPosition(vec3 p)
	}

	for (auto it = NewEntityTransfers.begin(); it != NewEntityTransfers.end();)
	{
		EntityTransferData& TransferData = it->second;
		// get the shard that owns that bound id
		const auto TargetShard = HeuristicManifest::Get().ShardFromBoundID(it->first);
		// if it does not exist then no shard owns this bound, or the bound is self, ignore this
		// transfer
		if (!TargetShard.has_value() || TargetShard.value() == NetworkCredentials::Get().GetID())
		{
			it = NewEntityTransfers.erase(it);	// erase returns the next valid iterator
			continue;
		}
		TransferData.ID = UUIDGen::Gen();
		TransferData.receiver = *TargetShard;
		TransferData.stage = EntityTransferStage::eNone;
		TransferData.WaitingOnResponse = false;
		EntityTransfers.insert(TransferData);
		for (const AtlasEntityID EntityID : TransferData.entityIDs)
		{
			EntitiesInTransfer.insert(std::make_pair(EntityID, TransferData.ID));
		}
		TransferManifest::Get().PushTransferInfo(TransferData);
		logger.DebugFormatted(
			"Scheduled new Entity Transfer:\n- ID {}\n- {} entities\n- Target: {}",
			UUIDGen::ToString(TransferData.ID), TransferData.entityIDs.size(),
			TransferData.receiver.ToString());

		++it;
	}
}
void TransferCoordinator::TransferTick()
{
	{
		std::lock_guard<std::mutex> lock(EntityTransferMutex);
		if (!EntityTransfers.empty())
		{
			const auto& StageView = EntityTransfers.get<TransferByStage>();
			const auto none_view = StageView.equal_range(EntityTransferStage::eNone);
			for (auto it = none_view.first; it != none_view.second; it++)
			{
				EntityTransferPacket etPacket;
				etPacket.TransferID = it->ID;
			}
		}
	}
}
