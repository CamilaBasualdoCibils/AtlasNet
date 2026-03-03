#pragma once

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <chrono>
#include <execution>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Client/Client.hpp"
#include "Client/Packet/ClientTransferPacket.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Transfer/TransferData.hpp"
#include "Entity/Transfer/TransferManifest.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
class TransferCoordinator : public Singleton<TransferCoordinator>
{
	struct TransferByID;
	struct TransferByStage;
	struct TransferByReceiver;

	boost::multi_index_container<
		EntityTransferData,
		boost::multi_index::indexed_by<
			boost::multi_index::hashed_unique<
				boost::multi_index::tag<TransferByID>,
				boost::multi_index::member<EntityTransferData, TransferID,
										   &EntityTransferData::ID>>,
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<TransferByStage>,
				boost::multi_index::member<EntityTransferData, EntityTransferStage,
										   &EntityTransferData::stage>>,
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<TransferByReceiver>,
				boost::multi_index::member<EntityTransferData, NetworkIdentity,
										   &EntityTransferData::shard>>>>
		EntityTransfers;
	struct EntityByEntityID;
	struct EntityByClientID;
	struct EntityByTransferID;
	std::unordered_map<AtlasEntityID, TransferID> EntitiesInTransfer;
	// std::unordered_map<TransferID, EntityTransferData> EntityTransfers;
	std::unordered_map<TransferID, ClientTransferData> ClientTransfers;

	std::stack<AtlasEntityID> EntitiesToParseForReceiver;
	mutable std::mutex EntityTransferMutex, ClientTransferMutex;
	std::jthread TransferThread;
	Log logger = Log("TransferCoordinator");
	PacketManager::Subscription EntityTransferPacketSubscription, ClientTransferPacketSubscription;

   public:
	TransferCoordinator();
	void MarkEntitiesForTransfer(const std::span<AtlasEntityID> entities);
	bool IsEntityInTransfer(AtlasEntityID ID) const;

   private:
	void TransferThreadEntry(std::stop_token st);

	void ParseEntitiesForTargets();
	void TransferTick();
	void OnEntityTransferPacketArrival(const EntityTransferPacket& p,
									   const PacketManager::PacketInfo& info);
	void OnClientTransferPacketArrival(const ClientTransferPacket& p,
									   const PacketManager::PacketInfo& info);
};