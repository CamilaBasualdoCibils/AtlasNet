#pragma once

#include <format>

#include "Client/Client.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkIdentity.hpp"
class RoutingManifest : public Singleton<RoutingManifest>
{
	const std::string ClientID_2_Proxy_Hashtable = "Routing::ClientID -> Proxy";
	constexpr static const char* Proxy_ClientIDs = "Routing::Proxy::{}_Clients";
	std::string GetProxyClientListKey(const NetworkIdentity& ID) const
	{
		return std::format(Proxy_ClientIDs, UUIDGen::ToString(ID.ID));
	}

   private:
   public:
	void GetProxyClients(const NetworkIdentity& id, std::vector<ClientID>& clients);
	std::optional<NetworkIdentity> GetClientProxy(const ClientID& client_ID);
	void AssignProxyClient(const ClientID& cid, const NetworkIdentity& ID);
};