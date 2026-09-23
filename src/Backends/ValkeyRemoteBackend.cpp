#include "ValkeyRemoteBackend.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/Node/DB/DebugMirror.hpp"
#include "AtlasNet/Node/DB/Keys.hpp"
#include <chrono>

AtlasNet::RPC::Database::RegisterNodeResponse
AtlasNet::DB::ValkeyRemoteBackend::RegisterNode(
    const RPC::Database::RegisterNodeRequest& request)
{
  NetBinaryWriter writer;
  auto record = request;
  writer(record);
  auto bytes = writer.Release();
  client.hset(AtlasNet::DB::Keys::RegisteredNodes, request.nodeID.to_string(),
              std::string_view(reinterpret_cast<const char*>(bytes.data()),
                               bytes.size()));
#ifdef DEBUG
  client.hset(
      AtlasNet::DB::Keys::DebugMirror(AtlasNet::DB::Keys::RegisteredNodes),
      request.nodeID.to_string(),
      AtlasNet::DB::DebugMirror::ToJSON(request).dump());
#endif
  return RPC::Database::RegisterNodeResponse::SUCCESS;
}

AtlasNet::RPC::Database::ClaimControllerPromotionResponse
AtlasNet::DB::ValkeyRemoteBackend::ClaimControllerPromotion(
    AtlasNetNodeID nodeID)
{
  constexpr auto claimDuration = std::chrono::seconds(30);
  const bool claimed = client.set(
      AtlasNet::DB::Keys::ControllerPromotion, nodeID.to_string(),
      claimDuration, sw::redis::UpdateType::NOT_EXIST);
  return claimed ? RPC::Database::ClaimControllerPromotionResponse::CLAIMED
                 : RPC::Database::ClaimControllerPromotionResponse::
                       ALREADY_CLAIMED;
}
