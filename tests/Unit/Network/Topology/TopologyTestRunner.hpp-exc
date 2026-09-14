#pragma once
#include "atlasnet/core/network/NetworkCommons.hpp"
#include "raylib-src/src/raylib.h"
#include <functional>
#include <gtest/gtest.h>
#include <queue>
using namespace AtlasNet::Network;
template <typename TTopology> class TopologyTestRunner : public ::testing::Test
{

  struct PacketHopAnimation
  {
    NetworkNodeID from;
    NetworkNodeID to;
    std::string message;
  };
  struct NodeAddedAnimation
  {
    NetworkNodeInfo node;
  };
  struct NodeRemovedAnimation
  {
    NetworkNodeID nodeId;
  };
  using AnimationEvent = std::variant<PacketHopAnimation, NodeAddedAnimation,
                                      NodeRemovedAnimation>;
  TTopology topology;
  std::queue<std::move_only_function<void()>> eventQueue;
  std::queue<AnimationEvent> animationEvents;

public:
  void Run();

  void EnqueueEvent(std::move_only_function<void()> event)
  {
    eventQueue.push(std::move(event));
  }
  void EnqueueAddNode(NetworkNodeInfo node)
  {
    EnqueueEvent(
        [this, node = std::move(node)]() mutable
        {
          // Add the node to the network
          animationEvents.push(NodeAddedAnimation{node});
        });
  }
  void EnqueueRemoveNode(NetworkNodeID nodeId)
  {
    EnqueueEvent(
        [this, nodeId]() mutable
        {
          // Remove the node from the network
          animationEvents.push(NodeRemovedAnimation{nodeId});
        });
  }
  void EnqueueApply()
  {
    EnqueueEvent(
        [this]() mutable
        {
          // Parse the topology
          topology.Parse();
        });
  }
  void EnqueueMessage(NetworkNodeID from, NetworkNodeID to, std::string message)
  {
    EnqueueEvent(
        [this, from = std::move(from), to = std::move(to),
         message = std::move(message)]() mutable
        {
          // Send a message from one node to another
          // For this example, we'll just print the message
          std::cout << "Message from " << from << " to " << to << ": "
                    << message << std::endl;
        });
  }

private:
  void _run_2d_scene()
  {
    InitWindow(1280, 720, "Network Topology Test");
    SetTargetFPS(60);
    Camera2D camera = {0};
    camera.target = {0.0f, 0.0f};
    camera.offset = {1280 * 0.5f, 720 * 0.5f};
    camera.zoom = 1.0f;
    while (!WindowShouldClose())
    {
      float dt = GetFrameTime();
      BeginDrawing();
      ClearBackground(Color{0, 0, 0, 255});
      BeginMode2D(camera);
      EndMode2D();
      EndDrawing();
    }
    CloseWindow();
  }
  void TestBody() override {}
};