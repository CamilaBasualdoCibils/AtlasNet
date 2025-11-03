// Minimal FTXUI visualization of a single partition with a moving dot entity.
// This guts the previous sample loop and renders a simple animated view.

#include "pch.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "AtlasNet/Client/AtlasNetClient.hpp"
#include "AtlasNet/Server/AtlasNetServer.hpp"
/**
 * @brief A simple entity that moves in a circular path.
 */
struct Entity
{
    float angle = 0.0f; // radians
    float speed = 0.8f; // radians per second
};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // Map/partition dimensions in terminal canvas pixels (approximate cells)
    const int map_width = 80;
    const int map_height = 24;

    // Local simulation state for our demo entity (ID 1). We will only
    // simulate it while we own it; otherwise we display the last authoritative state.
    constexpr AtlasEntityID kDemoEntityId = 1;
    MovingEntity entity;
    std::unordered_map<AtlasEntityID, AtlasEntity> local_entities; // what we render
    bool owns_demo_entity = true; // whether this partition currently owns ID 1

    // Time step
    const float dt = 1.0f / 60.0f; // 60 FPS tick

    // Screen setup
    auto screen = ScreenInteractive::Fullscreen();
    std::atomic<bool> running = true;

    // Initialize AtlasNet server side for snapshot forwarding to Partition
    AtlasNetServer::InitializeProperties initProps;
    initProps.ExePath = argv ? argv[0] : "SampleGame";
    initProps.OnShutdownRequest = [&](SignalType) {
        running = false;
        screen.Exit();
    };
    AtlasNetServer::Get().Initialize(initProps);

    // Mute logging to avoid interfering with the FTXUI canvas.
    Log::SetMuted(true);

    // Renderer draws a bordered partition and all local entities as dots, plus a log panel.
    auto renderer = Renderer([&]
    {
        Canvas c(map_width, map_height);

        // Draw partition border.
        for (int x = 0; x < map_width; ++x)
        {
            c.DrawPoint(x, 0, true);
            c.DrawPoint(x, map_height - 1, true);
        }
        for (int y = 0; y < map_height; ++y)
        {
            c.DrawPoint(0, y, true);
            c.DrawPoint(map_width - 1, y, true);
        }

        // Draw all known local entities from their world positions.
        for (const auto &kv : local_entities)
        {
            const auto &e = entities[i];
            std::cout << "Entity[" << i << "] pos = ("
                      << e.position.x << ", " << e.position.y << ")\n";
        }
    }
};
static std::unique_ptr<AtlasNetClient> g_client;
bool ShouldShutdown = false;
/**
 * @brief Main function to run the sample game.
 */
int main(int argc, char **argv)
{
    std::cerr << "SampleGame Starting" << std::endl;
    for (int i = 0; i < argc; i++)
    {
        std::cerr << argv[i] << std::endl;
    }
    AtlasNetServer::InitializeProperties InitProperties;
    AtlasNetServer::Get().Initialize(InitProperties);
    InitProperties.ExePath = argv[0];
    InitProperties.OnShutdownRequest = [&](SignalType signal)
    { ShouldShutdown = true; };

    Scene scene;
    // create an entity
    scene.entities.emplace_back(10.0f, 0.5f); // radius =10, slower

    // Time / loop variables
    using clock = std::chrono::high_resolution_clock;
    auto previous = clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));


        g_client = std::make_unique<AtlasNetClient>();
    AtlasNetClient::InitializeProperties props{
        .ExePath = "./AtlasNetClient.exe",
        //.ClientName = "SampleGame:" + DockerIO::Get().GetSelfContainerName(),
        .ClientName = "SampleGameClient",
        .ServerName = "GodViewServer"
    };
    //g_client->Initialize(props);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    while (!ShouldShutdown)
    {
      std::vector<AtlasEntity> Incoming;
      AtlasEntity entity;
      entity.ID = 0;
      entity.IsSpawned = true;
      Incoming.push_back(entity);
      std::span<AtlasEntity> myspan(Incoming);
      std::vector<AtlasEntityID> Outgoing;
      AtlasNetServer::Get().Update(myspan, Incoming, Outgoing);
      //g_client->SendEntityUpdate(entity);
        auto now = clock::now();
        std::chrono::duration<float> delta = now - previous;
        previous = now;
        float dt = delta.count(); // seconds

        scene.update(dt);

        // Print positions every second
        // scene.printPositions();

        // Sleep a bit to avoid burning CPU (simulate frame time)
        //std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // ~60 updates per second
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
