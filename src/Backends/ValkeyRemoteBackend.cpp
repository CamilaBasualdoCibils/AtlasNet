#include "ValkeyRemoteBackend.hpp"
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/DB/DebugMirror.hpp"
#include "AtlasNet/DB/Keys.hpp"

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
