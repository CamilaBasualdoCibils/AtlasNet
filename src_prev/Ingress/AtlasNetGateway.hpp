#pragma once

#include "atlasnet/client/ClientRPC.hpp"
#include "atlasnet/controller/ControllerRPC.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/client/ClientRegistry.hpp"

#include "atlasnet/core/entity/Entity.hpp"
#include "atlasnet/core/events/MessagingEvents.hpp"
#include "atlasnet/core/messages/HandshakePacket.hpp"
#include "atlasnet/core/node/AtlasNetNode.hpp"
#include "atlasnet/core/node/NodeTypes.hpp"
#include "atlasnet/gateway/GatewayRelayService.hpp"
#include "atlasnet/shard/ShardRPC.hpp"
#include <chrono>
#include <future>
#include <optional>
#include <vector>

namespace AtlasNet
{
class AtlasNetGateway : public IAtlasNetNode
{
public:
  AtlasNetGateway() : IAtlasNetNode(AtlasNetNodeType::Gateway) {}
  ~AtlasNetGateway() override = default;

  AtlasNetGatewayID GetGatewayID() const
  {
    assert(gatewayID_.has_value() && "Gateway ID not initialized");
    return gatewayID_.value();
  }

private:
  void OnInit() override;
  void OnShutdown() override {}

  HandshakeResponsePacket
  HandleHandshake(const HandshakeIdentity& identity,
                  const Network::SocketAddress& remoteAddr) override;
  void OnClientConnected(const ConnectionEstablishedEvent& event);

  std::optional<GatewayRelayService> gatewayRelayService_;
  std::optional<AtlasNetGatewayID> gatewayID_;
  
};
} // namespace AtlasNet