#pragma once
#include "AtlasNet/DB/Backend/IDatabaseBackend.hpp"
#include <sw/redis++/redis++.h>

namespace AtlasNet::DB
{
class ValkeyRemoteBackend final : public IDatabaseBackend
{
public:
  explicit ValkeyRemoteBackend(const std::string& uri) : client(uri) {}
  RPC::Database::RegisterNodeResponse
  RegisterNode(const RPC::Database::RegisterNodeRequest& request) override;

private:
  sw::redis::Redis client;
};
} // namespace AtlasNet::DB
