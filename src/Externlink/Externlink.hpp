#pragma once
#include "pch.hpp"
#include "Debug/Log.hpp"
#include <future>

/// A backend-agnostic connection identifier
struct ExternlinkConnection
{
    uint64_t id = 0;
    bool operator==(const ExternlinkConnection& other) const noexcept { return id == other.id; }
};

/// Externlink configuration
struct ExternlinkProperties
{
    std::string uuid;
    uint16_t port;
    bool tickAsyncOnStartListening = false;
};

/// @brief Lightweight, backend-agnostic networking interface built over Valve GameNetworkingSockets
class Externlink
{
public:
    using OnConnectedFn = std::function<void(const ExternlinkConnection&)>;
    using OnDisconnectedFn = std::function<void(const ExternlinkConnection&)>;
    using OnMessageFn = std::function<void(const ExternlinkConnection&, const std::string&)>;

    Externlink();
    ~Externlink();

    bool Start(const ExternlinkProperties& props);
    bool StartListening();
    std::future<void> PollAsync();
    void Poll();

    bool Externlink::Connect(const std::string& address, uint16_t port);

    void Send(const ExternlinkConnection& conn, const std::string& msg);
    void Broadcast(const ExternlinkConnection& sender, const std::string& msg);
    void Stop();

    void SetOnConnected(OnConnectedFn fn);
    void SetOnDisconnected(OnDisconnectedFn fn);
    void SetOnMessage(OnMessageFn fn);

private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("Externlink");

    ISteamNetworkingSockets* Interface = nullptr;
    HSteamListenSocket ListenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup PollGroup = k_HSteamNetPollGroup_Invalid;

    ExternlinkProperties Props;
    std::unordered_map<uint64_t, HSteamNetConnection> IdToConn;
    std::unordered_map<HSteamNetConnection, uint64_t> ConnToId;
    std::atomic<uint64_t> NextId = 1;

    // Callbacks
    OnConnectedFn OnConnected;
    OnDisconnectedFn OnDisconnected;
    OnMessageFn OnMessage;

    std::future<void> pollTask;
    std::atomic<bool> running = false;

    static Externlink* s_Instance;
    static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
};