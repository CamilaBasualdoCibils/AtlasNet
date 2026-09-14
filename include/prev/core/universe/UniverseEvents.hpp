#pragma once

#include "atlasnet/core/events/IEvent.hpp"
#include "atlasnet/core/universe/WorldConcepts.hpp"
#include <string>
namespace AtlasNet
{
ATLASNET_EVENT(WorldCreatedEvent, ATLASNET_EVENT_FIELD(std::string, worldName),
               ATLASNET_EVENT_FIELD(WorldID, worldID),
               ATLASNET_EVENT_FIELD(WorldDefinition, worldDefinition));
};