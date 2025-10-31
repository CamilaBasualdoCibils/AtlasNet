// Minimal FTXUI visualization of a single partition with a moving dot entity.
// This guts the previous sample loop and renders a simple animated view.

#include "pch.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "Debug/Log.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

struct MovingEntity
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

    // Entity state
    MovingEntity entity;

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

    // Renderer draws a bordered partition and a moving dot inside plus a log panel.
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

        // Compute dot position around full map bounds.
        const float cx = (map_width - 1) / 2.0f;
        const float cy = (map_height - 1) / 2.0f;
        const float rx = (map_width - 4) / 2.0f;  // padding from border
        const float ry = (map_height - 4) / 2.0f; // padding from border

        const float x = cx + rx * std::cos(entity.angle);
        const float y = cy + ry * std::sin(entity.angle);

        c.DrawPoint((int)std::round(x), (int)std::round(y), true);

        Element canvas_el = canvas(std::move(c));
        Element legend = hbox({
            text(" Single Partition ") | bold,
            separator(),
            text(" Dot = Entity "),
            separator(),
            text(" Ctrl+C to quit ")
        });

        return vbox({
            legend | center,
            canvas_el | border | size(WIDTH, EQUAL, map_width + 2) | size(HEIGHT, EQUAL, map_height + 2)
        });
    });

    // We avoid CatchEvent (may be unavailable). Advance state in ticker thread
    // and just use renderer as the root component.
    auto app = renderer;

    // Background ticker posting animation events.
    std::thread ticker([&]
    {
        using namespace std::chrono_literals;
        std::vector<AtlasEntity> incoming;           // not used yet, but kept for API symmetry
        std::vector<AtlasEntityID> outgoing_ids;     // not used yet
        while (running)
        {
            // Advance animation state and request redraw
            entity.angle += entity.speed * dt;
            const float two_pi = 2.0f * 3.14159265358979323846f;
            if (entity.angle > two_pi)
                entity.angle -= two_pi;
            screen.PostEvent(Event::Custom);

            // Convert moving dot to AtlasEntity and send snapshot to Partition via AtlasNet
            AtlasEntity atlas_entity{};
            atlas_entity.ID = 1;
            atlas_entity.IsSpawned = true;
            // Map terminal canvas coords into some world coordinates; keep z = 0
            {
                const float cx = (map_width - 1) / 2.0f;
                const float cy = (map_height - 1) / 2.0f;
                const float rx = (map_width - 4) / 2.0f;
                const float ry = (map_height - 4) / 2.0f;
                const float x = cx + rx * std::cos(entity.angle);
                const float y = cy + ry * std::sin(entity.angle);
                atlas_entity.Position.x = x;
                atlas_entity.Position.y = 0.0f;
                atlas_entity.Position.z = y;
            }

            std::vector<AtlasEntity> snapshot;
            snapshot.push_back(atlas_entity);
            std::span<AtlasEntity> snapshot_span(snapshot);
            AtlasNetServer::Get().Update(snapshot_span, incoming, outgoing_ids);
            std::this_thread::sleep_for(16ms); // ~60 FPS
        }
    });

    screen.Loop(app);

    running = false;
    if (ticker.joinable())
        ticker.join();

    return 0;
}
