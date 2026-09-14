#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Core/Network/NetworkCommons.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyDeployer.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyPlanner.hpp"
#include "AtlasNet/Core/Network/Topology/ITopologyPolicy.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyAgent.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyTransitionPlanner.hpp"
#include "AtlasNet/Core/Network/Topology/TransitionPlan.hpp"
#include "AtlasNet/Core/Network/Transport/Client/IClientTransport.hpp"
#include "raylib.h"

#include <glm/ext/vector_int2.hpp>
#include <gtest/gtest.h>
#include <list>
#include <memory>
#include <thread>

class TopologyTestAssigner
    : public AtlasNet::Network::Topology::ITopologyAssigner
{
public:
  TopologyTestAssigner() = default;
  struct TestCallbacks
  {
    std::function<void(AtlasNet::AtlasNetNodeID, AtlasNet::AtlasNetNodeID)>
        test_onConnect;
    std::function<void(AtlasNet::AtlasNetNodeID, AtlasNet::AtlasNetNodeID)>
        test_onDisconnect;
  };
  std::shared_ptr<TestCallbacks> GetCallbacks()
  {
    return callbacks;
  }

private:
  std::shared_ptr<TestCallbacks> callbacks = std::make_shared<TestCallbacks>(
      [this](AtlasNet::AtlasNetNodeID a, AtlasNet::AtlasNetNodeID b)
      { TriggerConnectionAssign(a, b); },
      [this](AtlasNet::AtlasNetNodeID a, AtlasNet::AtlasNetNodeID b)
      { TriggerDisconnectionAssign(a, b); });
};
class TopologyTestDeployer
    : public AtlasNet::Network::Topology::ITopologyDeployer
{

public:
  std::future<bool> Connect(AtlasNet::AtlasNetNodeID a, AtlasNet::AtlasNetNodeID b) override
  {

  }

  std::future<bool> Disconnect(AtlasNet::AtlasNetNodeID a, AtlasNet::AtlasNetNodeID b) override
  {

  }
};

class TopologyRenderer : public ::testing::Test
{
  size_t numNodes = 5;
  size_t numServers = 1;
  struct TestNode
  {
    AtlasNet::Network::NetworkNodeInfo info;
    std::shared_ptr<AtlasNet::Network::Topology::TopologyAgent> agent;
    std::shared_ptr<TopologyTestAssigner::TestCallbacks> assigner;
  };
  AtlasNet::Network::Topology::ConnectionGraph currentGraph;
  AtlasNet::Network::Topology::ConnectionGraph nextGraph;
  AtlasNet::Network::Topology::WeightGraph weightGraph;
  std::vector<TestNode> nodes;
  std::shared_ptr<AtlasNet::Network::IClientTransport> transport;
  std::shared_ptr<AtlasNet::Network::Topology::ITopologyPlanner> planner;
  std::shared_ptr<AtlasNet::Network::Topology::ITopologyDeployer> deployer;
  std::shared_ptr<AtlasNet::Network::Topology::ITopologyPolicy> policy;
  std::jthread renderThread;
  glm::ivec2 windowSize = {800, 600};
  Camera2D camera = {
      windowSize.x / 2.0f, windowSize.y / 2.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  std::optional<AtlasNet::Network::Topology::TransitionPlan>
      currentTransitionPlan;

private:
  void TestBody() override {}

public:
  TopologyRenderer() = default;
  void SetNumNodes(size_t n)
  {
    numNodes = n;
  }
  void SetNumServers(size_t n)
  {
    numServers = n;
  }
  void
  SetPlanner(std::shared_ptr<AtlasNet::Network::Topology::ITopologyPlanner> p)
  {
    planner = p;
  }
  void SetTransport(std::shared_ptr<AtlasNet::Network::IClientTransport> t)
  {
    transport = t;
  }
  void
  SetDeployer(std::shared_ptr<AtlasNet::Network::Topology::ITopologyDeployer> e)
  {
    deployer = e;
  }
  void
  SetPolicy(std::shared_ptr<AtlasNet::Network::Topology::ITopologyPolicy> p)
  {
    policy = p;
  }
  void Finish()
  {
    renderThread.request_stop();
    renderThread.join();
  }
  void Start()
  {
    renderThread = std::jthread(
        [this](std::stop_token st)
        {
          assert(numNodes > 0);
          assert(transport != nullptr);
          assert(planner != nullptr);
          assert(deployer != nullptr);
          assert(policy != nullptr);
          assert(numServers > 0);
          std::vector<std::string> Servers;
          for (size_t i = 0; i < numServers; i++)
          {
            Servers.push_back(std::format("Server {}", i));
          }
          for (size_t i = 0; i < numNodes; ++i)
          {
            std::unique_ptr<TopologyTestAssigner> assigner =
                std::make_unique<TopologyTestAssigner>();
            AtlasNet::Network::NetworkNodeInfo nodeInfo{
                AtlasNet::AtlasNetNodeID(AtlasNet::UUID::Generate()), 0.0f, Servers[i % Servers.size()],
                "rack" + std::to_string(i), "region" + std::to_string(i)};
            TestNode node;
            node.info = nodeInfo;
            node.assigner = assigner->GetCallbacks();
            node.agent =
                std::make_shared<AtlasNet::Network::Topology::TopologyAgent>(
                    transport, std::move(assigner));
            nodes.emplace_back(std::move(node));
          }
          Raylib_start();
          while (!st.stop_requested())
          {
            Raylib_render();
          }
          Raylib_stop();
        });
  }

  void RecomputeTopologyAndTransition()
  {
    weightGraph.Clear();
    for (size_t i = 0; i < nodes.size(); ++i)
    {
      AtlasNet::Network::NetworkNodeInfo nodeInfo = nodes[i].info;
      weightGraph.AddVertex(nodeInfo.nodeID, nodeInfo);
    }
    // for every node compute weight between every other node
    for (size_t i = 0; i < nodes.size(); ++i)
    {
      for (size_t j = i + 1; j < nodes.size(); ++j)
      {
        float cost = policy->ComputeCost(nodes[i].info, nodes[j].info);
        weightGraph.AddEdge(nodes[i].info.nodeID, nodes[j].info.nodeID, cost);
      }
    }
    auto connectionGraph = planner->Compute(weightGraph);
    nextGraph = connectionGraph;
    AtlasNet::Network::Topology::TopologyTransitionPlanner transitionPlanner;
    currentTransitionPlan =
        transitionPlanner.Compute(currentGraph, connectionGraph);
  }

  size_t StepsRemainingInTransition() const
  {
    if (currentTransitionPlan.has_value())
    {
      return currentTransitionPlan->StepsRemaining();
    }
    return 0;
  }
  void ExecuteNextTransitionStep()
  {
    if (currentTransitionPlan.has_value())
    {
      AtlasNet::Network::Topology::TransitionStep step =
          currentTransitionPlan->NextStep();

      for (const auto& connect : step.connect)
      {
        auto a = connect.a;
        auto b = connect.b;
        //deployer->Connect(nodes[a].info.nodeID, nodes[b].info.nodeID);
      }
      for (const auto& disconnect : step.disconnect)
      {
        auto a = disconnect.a;
        auto b = disconnect.b;
        //deployer->Disconnect(nodes[a].info.nodeID, nodes[b].info.nodeID);
      }
    }
  }

private:
  void Raylib_start()
  {
    InitWindow(800, 600, "Topology Renderer");
    SetTargetFPS(60);
  }
  void Raylib_stop()
  {
    CloseWindow();
  }
  void Raylib_render()
  {
    BeginDrawing();
    BeginMode2D(camera);
    ClearBackground(RAYWHITE);
    for (const auto& node : nodes)
    {
    }
    EndMode2D();
    EndDrawing();
  }
};