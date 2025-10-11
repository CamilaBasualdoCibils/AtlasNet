#include "Interlink/Interlink.hpp"

#include "Interlink.hpp"
#include "Database/ServerRegistry.hpp"
void Interlink::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState)
	{
		// Somebody is trying to connect
	case k_ESteamNetworkingConnectionState_Connecting:

		CallbackOnConnecting(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_Connected:
		CallbackOnConnected(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
		logger->Print("k_ESteamNetworkingConnectionState_ClosedByPeer");
		break;
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		logger->Print("k_ESteamNetworkingConnectionState_ProblemDetectedLocally");
		break;
	case k_ESteamNetworkingConnectionState_FinWait:
		logger->Print("k_ESteamNetworkingConnectionState_FinWait");
		break;

	case k_ESteamNetworkingConnectionState_Dead:
		logger->Print("k_ESteamNetworkingConnectionState_Dead");
		break;

	default:
		logger->Print(std::format("Unknown {}", (int64)pInfo->m_info.m_eState));
	};
}
void Interlink::SendMessage(const InterlinkMessage &message)
{
}
static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
	Interlink::Get().OnSteamNetConnectionStatusChanged(info);
}

void Interlink::GenerateNewConnections()
{
	auto &IndiciesByState = Connections.get<IndexByState>();
	auto PreConnectingConnections = IndiciesByState.equal_range(ConnectionState::ePreConnecting);
	// auto& ByState = Connections.equal_range(ConnectionState::ePreConnecting);
	SteamNetworkingConfigValue_t opt[1];
	opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				  (void *)SteamNetConnectionStatusChanged);
	for (auto it = PreConnectingConnections.first; it != PreConnectingConnections.second; ++it)
	{
		const Connection &connection = *it;
		logger->PrintFormatted("Connecting to {} at {}",connection.target.ToString(), connection.address.ToString());

		HSteamNetConnection conn =
			networkInterface->ConnectByIPAddress(connection.address.ToSteamIPAddr(), 1, opt);
		if (conn == k_HSteamNetConnection_Invalid)
		{
			logger->PrintFormatted("Failed to Generate New Connection {}", connection.address.ToString());
		}
		else
		{
			IndiciesByState.modify(it, [conn = conn](Connection &c)
								   { c.Connection = conn; });
		}
	}
}

void Interlink::OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
{
	logger->PrintFormatted(std::format("[GNS Debug] {}\n", pszMsg));
}

void Interlink::CallbackOnConnecting(SteamCBInfo info)
{

	auto &IndiciesBySteamConnection = Connections.get<IndexByHSteamNetConnection>();
	auto ExistingConnection = IndiciesBySteamConnection.find(info->m_hConn);

	// if connection is already registered then I initiated
	if (ExistingConnection != IndiciesBySteamConnection.end())
	{
		IndiciesBySteamConnection.modify(
			ExistingConnection, [](Connection &c)
			{ c.SetNewState(ConnectionState::eConnecting); });
		return;
	}
	else // if it is not then its not mine, this is a new incoming connection
	{
		// IdentityPacket identity = IdentityPacket::FromString(info->m_info.m_nUserData);
		Connection newCon;
		newCon.Connection = info->m_hConn;
		newCon.SetNewState(ConnectionState::eConnecting);
		newCon.address = IPAddress(info->m_info.m_addrRemote);
		const auto ID = InterLinkIdentifier::FromString(info->m_info.m_identityRemote.GetGenericString());
		if (info->m_info.m_identityRemote.m_eType != k_ESteamNetworkingIdentityType_GenericString)
		{
			logger->PrintFormatted("UNknown incoming connection {}. NetworkIdentity not a string");
			ASSERT(false, "Identity has to be string so we can identity by InterlinkIdentifier and server manifest");
		}
		if (!ID.has_value())
		{
			logger->PrintFormatted("unknown string in networkIdentity {}", info->m_info.m_identityRemote.GetGenericString());
			ASSERT(false, "Received connecting from something unknown");
		}
		if (!ServerRegistry::Get().ExistsInRegistry(ID.value()))
		{
			logger->PrintFormatted("Incoming connection from identity '{}' could not be verified since it was not in the registry", ID->ToString());
			ASSERT(false, "Received connecting from something not in the server registry");
		}
		logger->PrintFormatted("Incoming Connection from: {} at {}", ID->ToString(), newCon.address.ToString());
		newCon.target = ID.value();
		// request user for permission to connect
		if (EResult result = networkInterface->AcceptConnection(newCon.Connection);
			result != k_EResultOK)
		{

			logger->PrintFormatted("Error accepting connection: {}", uint64(result));
			networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
			throw std::runtime_error(
				"This exception is because I have not implemented what to do when it fails "
				"so i want to know when it does and not have undefined behaviour");
		}
		else
		{ // logger->Print("Establishing connection to ")
			Connections.insert(newCon);
		}
	}
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
	info->m_hConn;
	auto &indiciesBySteamConn = Connections.get<IndexByHSteamNetConnection>();
	if (auto v = indiciesBySteamConn.find(info->m_hConn); v != indiciesBySteamConn.end())
	{

		indiciesBySteamConn.modify(v, [](Connection &c)
								   { c.SetNewState(ConnectionState::eConnected); });
		networkInterface->SetConnectionPollGroup(v->Connection, PollGroup.value());
		logger->PrintFormatted(" - {} Connected", v->target.ToString());
	}
	else
	{
		ASSERT(false, "Connected? to non existent connection?");
	}
}

void Interlink::OpenListenSocket(PortType port)
{
	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			   (void *)SteamNetConnectionStatusChanged);
	SteamNetworkingIPAddr addr;
	addr.ParseString("0.0.0.0");
	addr.m_port = port;
	ListeningSocket = networkInterface->CreateListenSocketIP(
		addr, 1, &opt);
	if (ListeningSocket.value() == k_HSteamListenSocket_Invalid)
	{
		logger->PrintFormatted("Failed to listen on port {}", port);

		throw std::runtime_error(
			std::format("Failed to listen on port {}", port));
	}
	logger->PrintFormatted("Opened listen socket on port {}", port);
}

void Interlink::ReceiveMessages()
{
	ISteamNetworkingMessage *pIncomingMessages[32];
	int numMsgs = networkInterface->ReceiveMessagesOnPollGroup(PollGroup.value(), pIncomingMessages, 32);

	for (int i = 0; i < numMsgs; ++i)
	{
		ISteamNetworkingMessage *msg = pIncomingMessages[i];

		// Access message data
		const void *data = msg->m_pData;
		size_t size = msg->m_cbSize;
		HSteamNetConnection sender = msg->m_conn;

		// Example: interpret as string
		std::string text(reinterpret_cast<const char *>(data), size);
		std::cout << "Message from connection " << sender << ": " << text << std::endl;

		// Free message when done
		msg->Release();
	}
}

void Interlink::Init(const InterlinkProperties &Properties)
{

	MyIdentity = Properties.ThisID;
	ASSERT(Properties.logger, "Invalid Logger");
	logger = Properties.logger;

	logger->Print("Interlink init");
	ASSERT(MyIdentity.Type != InterlinkType::eInvalid, "Invalid Interlink Type");

	ASSERT(Properties.callbacks.acceptConnectionCallback,
		   "You must provide a function for accepting connections");
	callbacks = Properties.callbacks;
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{

		logger->Print(std::string("GameNetworkingSockets_Init failed: ") + errMsg);
		return;
	}
	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Msg,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
		{
			Interlink::Get().OnDebugOutput(eType, pszMsg);
		});
	networkInterface = SteamNetworkingSockets();
	SteamNetworkingIdentity identity;
	identity.SetGenericString(MyIdentity.ToString().c_str());
	networkInterface->ResetIdentity(&identity);
	PollGroup = networkInterface->CreatePollGroup();
	switch (MyIdentity.Type)
	{
	case InterlinkType::eGameServer:
	{
		OpenListenSocket(_PORT_GAMESERVER);
		InterLinkIdentifier PartitionID = MyIdentity;
		PartitionID.Type = InterlinkType::ePartition;
		EstablishConnectionTo(PartitionID);
	}
	break;
	case InterlinkType::ePartition:
		OpenListenSocket(_PORT_PARTITION);
		EstablishConnectionTo(InterLinkIdentifier::MakeIDGod());

		break;
	case InterlinkType::eGod:
		OpenListenSocket(_PORT_GOD);

		break;
	default:
		break;
	}
}

void Interlink::Shutdown()
{
	logger->Print("Interlink Shutdown");
}

bool Interlink::EstablishConnectionTo(const InterLinkIdentifier &id)
{
	if (Connections.get<IndexByTarget>().contains(id))
	{
		return true;
	}
	auto IP = ServerRegistry::Get().GetIPOfID(id);

	if (!IP.has_value())
		return false;
	Connection connec;
	SteamNetworkingIPAddr addr = IP->ToSteamIPAddr();
	addr.m_port = _PORT_GOD;
	// std::string s = std::string(":")+ std::to_string(_PORT_GOD);
	// addr.ParseString(s.c_str());
	IPAddress ip2(addr);

	logger->PrintFormatted("Establishing connection to {}", id.ToString());
	connec.address = IP.value();
	connec.target = id;
	connec.SetNewState(ConnectionState::ePreConnecting);
	// connections.push_back(connection);
	// AddToConnections(connection);
	Connections.insert(connec);
	return true;
}

void Interlink::Tick()
{
	GenerateNewConnections();
	networkInterface->RunCallbacks(); // process events
									  // logger->Print("tick");
}
