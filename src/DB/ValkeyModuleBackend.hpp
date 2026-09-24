#pragma once
#include "AtlasNet/Node/DB/Backend/IDatabaseBackend.hpp"
#include "valkeymodule.h"

namespace AtlasNet::DB
{
// Non-owning context. Calls must run on the Valkey event-loop thread.
class ValkeyModuleBackend final : public IDatabaseBackend
{
public:
  explicit ValkeyModuleBackend(ValkeyModuleCtx* context) : context(context) {}
  RPC::Database::RegisterNodeResponse
  RegisterNode(const RPC::Database::RegisterNodeRequest& request) override;
  RPC::Database::ClaimControllerPromotionResponse
  ClaimControllerPromotion(AtlasNetNodeID nodeID) override;

private:
  ValkeyModuleCtx* context;
};
} // namespace AtlasNet::DB
