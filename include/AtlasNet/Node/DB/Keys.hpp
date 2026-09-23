#pragma once

#include <string>
#include <string_view>

namespace AtlasNet::DB::Keys
{
inline constexpr std::string_view RegisteredNodes = "AtlasNet:RegisteredNodes";
inline constexpr std::string_view ControllerPromotion =
    "AtlasNet:ControllerPromotion";

inline std::string DebugMirror(std::string_view key)
{
  return std::string(key) + "_debug";
}
} // namespace AtlasNet::DB::Keys
