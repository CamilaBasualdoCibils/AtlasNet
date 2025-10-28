#include "Externlink.hpp"

Externlink* Externlink::s_Instance = nullptr;

Externlink::Externlink() {}
Externlink::~Externlink() { Stop(); }

bool Externlink::Start(const ExternlinkProperties& props)
{
    Props = props;
    s_Instance = this;

    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
    {
        logger->ErrorFormatted("[Externlink] Init failed: {}", errMsg);
        return false;
    }

    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(SteamNetConnectionStatusChanged);
    Interface = SteamNetworkingSockets();

    logger->DebugFormatted("[Externlink] Initialized on port {}", Props.port);
    return true;
}

bool Externlink::StartListening()
{
    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = Props.port;

    ListenSocket = Interface->CreateListenSocketIP(addr, 0, nullptr);
    PollGroup = Interface->CreatePollGroup();

    if (ListenSocket == k_HSteamListenSocket_Invalid)
    {
        logger->ErrorFormatted("[Externlink] Failed to create listen socket!");
        return false;
    }

    logger->DebugFormatted("[Externlink] Listening on port {}", Props.port);

    if (Props.tickAsyncOnStartListening)
    {
      logger->DebugFormatted("[Externlink] Polling async on port {}", Props.port);
      pollTask = PollAsync();
    }

    return true;
}

std::future<void> Externlink::PollAsync()
{
    running = true;
    return std::async(std::launch::async, [this]()
    {
        while (running)
        {
            Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void Externlink::Poll()
{
    Interface->RunCallbacks();

    ISteamNetworkingMessage* msgs[32];
    int num = Interface->ReceiveMessagesOnPollGroup(PollGroup, msgs, 32);

    for (int i = 0; i < num; ++i)
    {
        ISteamNetworkingMessage* msg = msgs[i];
        auto it = ConnToId.find(msg->m_conn);
        if (it == ConnToId.end())
        {
            msg->Release();
            continue;
        }

        ExternlinkConnection wrapped{it->second};
        if (OnMessage)
            OnMessage(wrapped, std::string((char*)msg->m_pData, msg->m_cbSize));

        msg->Release();
    }
    
    if (num > 0)
        logger->DebugFormatted("[Externlink] Polled: received {} messages", num);
}

bool Externlink::Connect(const std::string& address, uint16_t port)
{
    if (!Interface)
    {
        SteamDatagramErrMsg errMsg;
        if (!GameNetworkingSockets_Init(nullptr, errMsg))
        {
            logger->ErrorFormatted("[Externlink] Init failed: {}", errMsg);
            return false;
        }
        Interface = SteamNetworkingSockets();
        SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(SteamNetConnectionStatusChanged);
        s_Instance = this;
    }

    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.ParseString(address.c_str());
    addr.m_port = port;

    HSteamNetConnection conn = Interface->ConnectByIPAddress(addr, 0, nullptr);
    if (conn == k_HSteamNetConnection_Invalid)
    {
        logger->ErrorFormatted("[Externlink] Failed to connect to server {}:{}", address, port);
        return false;
    }

    logger->DebugFormatted("[Externlink] Connecting to {}:{}", address, port);
    if (Props.tickAsyncOnStartListening)
        pollTask = PollAsync();

    return true;
}


void Externlink::Send(const ExternlinkConnection& conn, const std::string& msg)
{
    auto it = IdToConn.find(conn.id);
    if (it == IdToConn.end())
        return;
    Interface->SendMessageToConnection(it->second, msg.data(), (uint32_t)msg.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void Externlink::Broadcast(const ExternlinkConnection& sender, const std::string& msg)
{
    for (auto& [id, conn] : IdToConn)
    {
        if (id == sender.id)
            continue;
        Interface->SendMessageToConnection(conn, msg.data(), (uint32_t)msg.size(), k_nSteamNetworkingSend_Reliable, nullptr);
    }
}

void Externlink::Stop()
{
    running = false;
    if (pollTask.valid())
        pollTask.wait();

    if (Interface)
    {
        if (ListenSocket != k_HSteamListenSocket_Invalid)
            Interface->CloseListenSocket(ListenSocket);
        if (PollGroup != k_HSteamNetPollGroup_Invalid)
            Interface->DestroyPollGroup(PollGroup);
    }
    GameNetworkingSockets_Kill();
    std::cout << "[Externlink] Shutdown complete." << std::endl;
}

void Externlink::SetOnConnected(OnConnectedFn fn) { OnConnected = std::move(fn); }
void Externlink::SetOnDisconnected(OnDisconnectedFn fn) { OnDisconnected = std::move(fn); }
void Externlink::SetOnMessage(OnMessageFn fn) { OnMessage = std::move(fn); }

void Externlink::SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (!s_Instance) return;
    auto* self = s_Instance;
    auto* iface = self->Interface;

    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_Connecting:
        {
            iface->AcceptConnection(info->m_hConn);
            iface->SetConnectionPollGroup(info->m_hConn, self->PollGroup);

            uint64_t id = self->NextId++;
            self->IdToConn[id] = info->m_hConn;
            self->ConnToId[info->m_hConn] = id;

            if (self->OnConnected)
                self->OnConnected(ExternlinkConnection{id});

            s_Instance->logger->DebugFormatted("[Externlink] Client connected (id={})", id);
            break;
        }

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            uint64_t id = self->ConnToId[info->m_hConn];
            self->IdToConn.erase(id);
            self->ConnToId.erase(info->m_hConn);

            if (self->OnDisconnected)
                self->OnDisconnected(ExternlinkConnection{id});

            iface->CloseConnection(info->m_hConn, 0, nullptr, false);
            s_Instance->logger->DebugFormatted("[Externlink] Client disconnected (id={})", id);
            break;
        }

        default:
            break;
    }
}