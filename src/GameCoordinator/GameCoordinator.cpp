#include "GameCoordinator.hpp"
#include <thread>
#include <chrono>

GameCoordinator::GameCoordinator() {}
GameCoordinator::~GameCoordinator() { Shutdown(); }

bool GameCoordinator::Init()
{
  uint16_t port = 9000;
    ExternlinkProperties props = {"Coordinator", port, true};
    if (!Link.Start(props))
        return false;

    Link.SetOnConnected([this](const ExternlinkConnection& conn) { OnClientConnected(conn); });
    Link.SetOnDisconnected([this](const ExternlinkConnection& conn) { OnClientDisconnected(conn); });
    Link.SetOnMessage([this](const ExternlinkConnection& conn, const std::string& msg) { OnClientMessage(conn, msg); });

    if (!Link.StartListening())
        return false;

    logger->DebugFormatted("[Coordinator] Initialized and listening on port {}", port);
    return true;
}

void GameCoordinator::Run()
{
    while (!ShouldShutdown)
    {
        Link.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GameCoordinator::Shutdown()
{
    ShouldShutdown = true;
    Link.Stop();
}

void GameCoordinator::OnClientConnected(const ExternlinkConnection& conn)
{
    Clients[conn.id] = "Client-" + std::to_string(conn.id);
    logger->DebugFormatted("[Coordinator] Connected: {}", conn.id);
}

void GameCoordinator::OnClientDisconnected(const ExternlinkConnection& conn)
{
    Clients.erase(conn.id);
    logger->DebugFormatted("[Coordinator] Disconnected: {}", conn.id);
}

void GameCoordinator::OnClientMessage(const ExternlinkConnection& conn, const std::string& msg)
{
    logger->DebugFormatted("[Coordinator] Message from {}: {}", conn.id, msg);
    Link.Broadcast(conn, msg);
}