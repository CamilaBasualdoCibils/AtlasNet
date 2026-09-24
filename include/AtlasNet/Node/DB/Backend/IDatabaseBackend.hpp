#pragma once
#include "AtlasNet/Node/RPC/Database.hpp"

namespace AtlasNet::DB
{
// Registry storage only. The host supplies local Module API or remote RESP
// access.
class IDatabaseBackend
{
public:
  virtual ~IDatabaseBackend() = default;
  virtual RPC::Database::RegisterNodeResponse
  RegisterNode(const RPC::Database::RegisterNodeRequest& request) = 0;
  virtual RPC::Database::ClaimControllerPromotionResponse
  ClaimControllerPromotion(AtlasNetNodeID nodeID) = 0;
};
} // namespace AtlasNet::DB
