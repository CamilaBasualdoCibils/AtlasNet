#pragma once

#include <optional>
#include "Client/Client.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkIdentity.hpp"
class ClientManifest : public Singleton<ClientManifest>
{
	const std::string ClientID_2_IP_Hashtable = "Client::ClientID -> IP";
	const std::string ClientID_2_EntityID_Hashtable = "Client::ClientID -> EntityID";

   private:
	void __ClientSetIP(const ClientID& c, const IPAddress& address);
	
	public:
	 std::optional<AtlasEntityID> GetClientEntityID(const ClientID& clientid);
	 void InsertClient(const Client& client);
	 void AssignClientEntity(const ClientID& clientid, const AtlasEntityID& entityid);
	 void RemoveClient(const ClientID& clientID);
};