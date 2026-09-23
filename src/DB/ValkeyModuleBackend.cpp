#define VALKEYMODULE_API extern
#include "ValkeyModuleBackend.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/Node/DB/DebugMirror.hpp"
#include "AtlasNet/Node/DB/Keys.hpp"

namespace
{
bool HashSet(ValkeyModuleCtx* context, std::string_view key,
             std::string_view field, std::string_view value)
{
  auto* reply =
      ValkeyModule_Call(context, "HSET", "bbb!", key.data(), key.size(),
                        field.data(), field.size(), value.data(), value.size());
  const bool success =
      reply && ValkeyModule_CallReplyType(reply) == VALKEYMODULE_REPLY_INTEGER;
  if (reply)
    ValkeyModule_FreeCallReply(reply);
  return success;
}
} // namespace

AtlasNet::RPC::Database::RegisterNodeResponse
AtlasNet::DB::ValkeyModuleBackend::RegisterNode(
    const RPC::Database::RegisterNodeRequest& request)
{
  NetBinaryWriter writer;
  auto record = request;
  writer(record);
  auto bytes = writer.Release();
  const auto id = request.nodeID.to_string();
  const std::string_view binaryValue(
      reinterpret_cast<const char*>(bytes.data()), bytes.size());
  const bool success =
      HashSet(context, AtlasNet::DB::Keys::RegisteredNodes, id, binaryValue);
#ifdef DEBUG
  const auto debugKey =
      AtlasNet::DB::Keys::DebugMirror(AtlasNet::DB::Keys::RegisteredNodes);
  const auto debugValue = AtlasNet::DB::DebugMirror::ToJSON(request).dump();
  const bool debugSuccess = HashSet(context, debugKey, id, debugValue);
  if (!debugSuccess)
    return RPC::Database::RegisterNodeResponse::FAILURE;
#endif
  return success ? RPC::Database::RegisterNodeResponse::SUCCESS
                 : RPC::Database::RegisterNodeResponse::FAILURE;
}

AtlasNet::RPC::Database::ClaimControllerPromotionResponse
AtlasNet::DB::ValkeyModuleBackend::ClaimControllerPromotion(
    AtlasNetNodeID nodeID)
{
  const auto owner = nodeID.to_string();
  constexpr std::string_view expiration = "30000";
  auto* reply = ValkeyModule_Call(
      context, "SET", "bbccc!", AtlasNet::DB::Keys::ControllerPromotion.data(),
      AtlasNet::DB::Keys::ControllerPromotion.size(), owner.data(), owner.size(),
      "NX", "PX", expiration.data());
  if (!reply)
    return RPC::Database::ClaimControllerPromotionResponse::FAILURE;
  const auto type = ValkeyModule_CallReplyType(reply);
  ValkeyModule_FreeCallReply(reply);
  if (type == VALKEYMODULE_REPLY_STRING)
    return RPC::Database::ClaimControllerPromotionResponse::CLAIMED;
  if (type == VALKEYMODULE_REPLY_NULL)
    return RPC::Database::ClaimControllerPromotionResponse::ALREADY_CLAIMED;
  return RPC::Database::ClaimControllerPromotionResponse::FAILURE;
}
