#define VALKEYMODULE_API extern
#include "ValkeyModuleBackend.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/DB/DebugMirror.hpp"
#include "AtlasNet/DB/Keys.hpp"

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
