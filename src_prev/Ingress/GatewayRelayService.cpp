#include "atlasnet/gateway/GatewayRelayService.hpp"
#include "AtlasNetGateway.hpp"
void AtlasNet::GatewayRelayService::DeclareGatewayRelay(const AtlasNet::AtlasNetClientID& clientID)
{
  ByteWriter clientIdWriter;
  clientIdWriter(clientID);
  ByteWriter gatewayIdWriter;
  gatewayIdWriter(config_.gateway->GetGatewayID());

  // Check if the client already has an associated gateway and remove it from
  // the sorted set if it exists
  if (std::optional<std::string> existingGatewayID =
          config_.redisConn->HashMap().GetSet().HGet(
              ClientID2GatewayID, clientIdWriter.as_string_view()))
  {
    ByteReader existingGatewayIDReader(existingGatewayID.value());
    AtlasNetGatewayID existingGatewayIDParsed;
    existingGatewayIDReader(existingGatewayIDParsed);
    config_.redisConn->Set().Modify().SRem(
        GatewayID2ClientIDs_SetKey(existingGatewayIDParsed),
        clientIdWriter.as_string_view());

    if (Env::DebugMode)
    {
      config_.redisConn->Set().Modify().SRem(
          GatewayID2ClientIDs_SetKey(existingGatewayIDParsed) + "_debug",
          clientID.to_string());
    }
  }
  // Set the new gateway for the client
  config_.redisConn->HashMap().GetSet().HSet(ClientID2GatewayID,
                                             clientIdWriter.as_string_view(),
                                             gatewayIdWriter.as_string_view());

  // Add the client to the set of clients for the new gateway
  config_.redisConn->Set().Modify().SAdd(
      GatewayID2ClientIDs_SetKey(config_.gateway->GetGatewayID()),
      clientIdWriter.as_string_view());
  if (Env::DebugMode)
  {
    config_.redisConn->HashMap().GetSet().HSet(
        ClientID2GatewayID + "_debug", clientID.to_string(),
        config_.gateway->GetGatewayID().to_string());
    config_.redisConn->Set().Modify().SAdd(
        GatewayID2ClientIDs_SetKey(config_.gateway->GetGatewayID()) + "_debug",
        clientID.to_string());
  }
  logger->info(
      "Declared gateway relay: ClientID {} is now managed by GatewayID {}",
      clientID.to_string(), config_.gateway->GetGatewayID().to_string());
}
