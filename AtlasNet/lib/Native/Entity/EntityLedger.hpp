#pragma once

#include <boost/container/flat_map.hpp>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "Network/Packet/PacketManager.hpp"
class EntityLedger : public Singleton<EntityLedger>
{
	boost::container::flat_map<AtlasEntityID, AtlasEntity> entities;

	PacketManager::Subscription sub_EntityListRequestPacket;
	Log logger = Log("EntityLedger");
   public:
	void Init();

	auto ViewLocalEntities()
	{
		return std::ranges::transform_view(entities,  // underlying range
										   [](auto& kv) -> AtlasEntity&
										   {  // projection lambda
											   return kv.second;
										   });
	}
	void RegisterNewEntity(const AtlasEntity& e)
	{
		ASSERT(!entities.contains(e.Entity_ID), "Duplicate Entities");
		entities.insert(std::make_pair(e.Entity_ID, e));
	}
	void OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
								  const PacketManager::PacketInfo& info);
};