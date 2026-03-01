#include "ClientManifest.hpp"

#include <boost/container/small_vector.hpp>
#include <optional>

#include "Client/Client.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"

void ClientManifest::__ClientSetIP(const ClientID& c, const IPAddress& address)
{
	ByteWriter IDWrite;
	ByteWriter IPWrite;
	IDWrite.uuid(c);
	address.Serialize(IPWrite);
	const auto result = InternalDB::Get()->HSet(ClientID_2_IP_Hashtable, IDWrite.as_string_view(),
												IPWrite.as_string_view());
}

void ClientManifest::InsertClient(const Client& client)
{
	__ClientSetIP(client.ID, client.ip);
}
void ClientManifest::RemoveClient(const ClientID& clientID)
{
	ByteWriter bw;
	bw.uuid(clientID);
	const auto result1 = InternalDB::Get()->HDel(ClientID_2_IP_Hashtable, {bw.as_string_view()});
	const auto result2 =
		InternalDB::Get()->HDel(ClientID_2_EntityID_Hashtable, {bw.as_string_view()});
}
void ClientManifest::AssignClientEntity(const ClientID& clientid, const AtlasEntityID& entityid)
{
	ByteWriter keyWrite;
	ByteWriter valueWrite;
	keyWrite.uuid(clientid);
	valueWrite.uuid(entityid);
	const auto result = InternalDB::Get()->HSet(
		ClientID_2_EntityID_Hashtable, keyWrite.as_string_view(), valueWrite.as_string_view());
}
std::optional<AtlasEntityID> ClientManifest::GetClientEntityID(const ClientID& clientid)
{
	ByteWriter keyWrite;
	keyWrite.uuid(clientid);
	const std::optional<std::string> value =
		InternalDB::Get()->HGet(ClientID_2_EntityID_Hashtable, keyWrite.as_string_view());
	if (!value.has_value())
		return std::nullopt;

	ByteReader br(*value);
	return br.uuid();
}
