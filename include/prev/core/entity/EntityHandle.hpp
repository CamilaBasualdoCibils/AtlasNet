#pragma once
#include "atlasnet/core/CmdSig/command/Command.hpp"
#include "raylib-src/src/raylib.h"
#include <string_view>

namespace AtlasNet
{
class EntityHandle
{
public:
  void SendCommand(const std::string_view& commandName,
                   const std::vector<uint8_t>& payload) {

  };


  Transform GetCurrentTransform() const
  {
    return Transform();
  };
};
} // namespace AtlasNet