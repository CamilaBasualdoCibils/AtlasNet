#include "GameCoordinator.hpp"
#include <thread>
#include <chrono>

GameCoordinator::GameCoordinator() {}
GameCoordinator::~GameCoordinator() { Shutdown(); }

bool GameCoordinator::Init()
{
  uint16_t port = 9000;
    ExternlinkProperties props = {"Coordinator", port};
    if (!Link.Start(props))
        return false;

    Link.SetOnConnected([this](const ExternlinkConnection& conn) { OnClientConnected(conn); });
    Link.SetOnDisconnected([this](const ExternlinkConnection& conn) { OnClientDisconnected(conn); });
    Link.SetOnMessage([this](const ExternlinkConnection& conn, const std::string& msg) { OnClientMessage(conn, msg); });

    if (!Link.StartListening())
        return false;

    std::cout << "[Coordinator] Initialized and listening on port " << port << std::endl;
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
    std::cout << "[Coordinator] Connected: " << conn.id << std::endl;
}

void GameCoordinator::OnClientDisconnected(const ExternlinkConnection& conn)
{
    Clients.erase(conn.id);
    std::cout << "[Coordinator] Disconnected: " << conn.id << std::endl;
}

void GameCoordinator::OnClientMessage(const ExternlinkConnection& conn, const std::string& msg)
{
    std::cout << "[Coordinator] Message from " << conn.id << ": " << msg << std::endl;
    Link.Broadcast(conn, msg);
}