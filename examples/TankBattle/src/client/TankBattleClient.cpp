
/* #include "TankBattleClient.hpp"
#include "Commands.hpp"
#include "atlasnet/core/CmdSig/command/CommandEnums.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "boost/describe/enum_to_string.hpp" */
int main()
{
  /* TankBattle::TankBattleClient client;
  client.Run(); */
  return 0;
}
/* void TankBattle::TankBattleClient::Run()
{
  InitWindow(800, 600, "Tank Battle");
  SetTargetFPS(120);

  AtlasNetClient_Init();
  BeginDrawing();
  ClearBackground(RAYWHITE);
  DrawText("Connecting to AtlasNet Server", 190, 200, 20, LIGHTGRAY);
  EndDrawing();
  AtlasNet::IAtlasNetClient::AtlasNetClientError error;
  std::string errorMessage;
  if (!AtlasNetClient_Connect("172.17.0.1", 42000, &error, &errorMessage))
  {
    logger->error("Failed to connect to AtlasNet server: {}", errorMessage);
   
    throw std::runtime_error("Failed to connect to AtlasNet server: " +
                             errorMessage);
    return;
  }
  else
  {
    logger->info("Successfully connected to AtlasNet server.");
  }
   logger->info("Dispatching PlayerMoveCommand to server.");
  PlayerMoveCommand moveCommand;
  moveCommand.delta = {0.01f, 0.0f}; // Example movement delta
  AtlasNet::CommandAckStatus status = AtlasNetClient_DispatchCommand(moveCommand,
                                 AtlasNet::CommandDeliveryGuarantee::ServerConfirmed);
  logger->info("Dispatched PlayerMoveCommand to server. with status {}", boost::describe::enum_to_string(status, "<INVALID>"));
  while (!WindowShouldClose())
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Connected to AtlasNet Server!", 190, 200, 20, LIGHTGRAY);

    EndDrawing();
  }
} */
