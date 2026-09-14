#pragma once

#include "AtlasNet/Core/Core.hpp"
#include "AtlasNet/Node/NodeData.hpp"
namespace AtlasNet
{
    struct ControllerInfo
    {
        NodeData nodeData;

        _Json to_json() const
        {
            return _Json{
                {"nodeData", nodeData.to_json()},
            };
        }
        void from_json(const _Json& j)
        {
            nodeData.from_json(j.at("nodeData"));
        }
    };
} // namespace AtlasNet