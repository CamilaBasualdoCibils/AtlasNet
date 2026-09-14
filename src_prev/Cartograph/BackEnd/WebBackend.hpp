#pragma once 


#include "EntityStream/EntityStreamWebSockController.hpp"
#include "atlasnet/core/node/AtlasNetNode.hpp"
#include <drogon/drogon.h>

#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
namespace AtlasNet
{

class CartographBackendService : public IAtlasNetNode
{

  CartographBackendService() : IAtlasNetNode(AtlasNetNodeType::Cartograph) {}
  public:
  static CartographBackendService& GetInstance()
  {
    static CartographBackendService instance;
    return instance;
  }
  using IAtlasNetNode::GetTaskSystem;
  using IAtlasNetNode::GetMessageSystem;
  using IAtlasNetNode::GetRPCSystem;
  using IAtlasNetNode::GetNodeRegistry;
  using IAtlasNetNode::GetLogger;
  using IAtlasNetNode::GetAddressResolver;
private:
  void OnInit() override;

  void OnShutdown() override;
  void Setup();

private:
  std::optional<EntityStreamWebSockController*> entityStreamWebSock;
};
} // namespace AtlasNet
