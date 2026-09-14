#pragma once
#include "atlasnet/core/CmdSig/command/CommandEnums.hpp"
#include "atlasnet/core/CoreDefs.hpp"
#include "atlasnet/core/RPC/RPCSystem.hpp"
#include "atlasnet/core/tasks/TaskSystem.hpp"
#include <chrono>
#include <future>
namespace AtlasNet
{


class CommandDispatcher
{
  struct Config
  {
    TaskSystem* taskSystem = nullptr;
    RPCSystem* rpcSystem = nullptr;
  };
  CommandDispatcher(const Config& config);
  template <typename CommandType>
  std::future<CommandAck>
  Dispatch(const CommandType& command, AtlasNetEntityID targetEntity,
           CommandDeliveryGuarantee guarante,
           std::optional<std::chrono::milliseconds> timeout =
               std::chrono::seconds(5))
  {
    std::promise<CommandAck> promise;
    config_.rpcSystem-
  }


  private:
  Config config_;
};
}; // namespace AtlasNet