#include "RoutingManifest.hpp"

#include <boost/container/small_vector.hpp>
#include <optional>

#include "Client/Client.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"

void RoutingManifest::GetProxyClients(const NetworkIdentity& id, std::vector<ClientID>& clients)
{
	const std::string Key = GetProxyClientListKey(id);
	const std::vector<std::string> members = InternalDB::Get()->SMembers(Key);
	clients.clear();
	for (const auto& m : members)
	{
		ByteReader br(m);
		clients.emplace_back(br.uuid());
	}
}
std::optional<NetworkIdentity> RoutingManifest::GetClientProxy(const ClientID& client_ID)
{
	ByteWriter keyWrite;
	keyWrite.uuid(client_ID);
	const std::optional<std::string> ret =
		InternalDB::Get()->HGet(ClientID_2_Proxy_Hashtable, keyWrite.as_string_view());
	if (!ret.has_value())
		return std::nullopt;
	ByteReader br(*ret);
	return NetworkIdentity::MakeIDProxy(br.uuid());
}
void RoutingManifest::AssignProxyClient(const ClientID& cid, const NetworkIdentity& ID)
{
	{
		const std::string ClientListKey = GetProxyClientListKey(ID);
		ByteWriter valueWrite;
		valueWrite.uuid(cid);
		InternalDB::Get()->SAdd(ClientListKey, {valueWrite.as_string_view()});
	}
	{
		ByteWriter keywrite, valuewrite;
		keywrite.uuid(cid);
		valuewrite.uuid(ID.ID);
		const auto ret = InternalDB::Get()->HSet(
			ClientID_2_Proxy_Hashtable, keywrite.as_string_view(), valuewrite.as_string_view());
	}
}
