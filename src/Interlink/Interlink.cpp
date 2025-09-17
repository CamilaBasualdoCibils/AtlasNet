#include "Interlink/Interlink.hpp"

void Interlink::Initialize(const InterlinkProperties& Properties)
{
    std::cerr << "Interlink::Initialize called\n";
    ThisType = Properties.Type;
    ASSERT(ThisType!= InterlinkType::eInvalid, "Invalid Interlink Type");
    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
    {
        std::cerr << "GameNetworkingSockets_Init failed: " << errMsg << std::endl;
        return;
    }
    networkInterface = SteamNetworkingSockets();
    SteamNetworkingIPAddr serverLocalAddr;
		serverLocalAddr.Clear();

    if (ThisType == InterlinkType::ePartition)
    {
        //try to connect to game server
        serverLocalAddr.SetIPv6LocalHost(CJ_GAMESERVER_PORT);
        HSteamNetConnection connection = networkInterface->ConnectByIPAddress(serverLocalAddr, 0, nullptr);
        ASSERT(connection != k_HSteamNetConnection_Invalid, "Failed to create connection to game server");
        std::shared_ptr<Connection> conn = std::make_shared<Connection>();
    }
    else if (ThisType == InterlinkType::eGameServer)
    {
        //try to listen for an incoming connection from partition
        serverLocalAddr.SetIPv6LocalHost(CJ_PARTITION_PORT);
        HSteamListenSocket listenSocket = networkInterface->CreateListenSocketIP(serverLocalAddr, 0, nullptr);
        ASSERT(listenSocket != k_HSteamListenSocket_Invalid, "Failed to create listen socket");
        std::shared_ptr<Connection> conn = std::make_shared<Connection>();
        OpenConnections[InterlinkType::ePartition] = conn;
    }

    //if()(ThisType == InterlinkType::eGod)
        //try to listen for partiions 
    
}