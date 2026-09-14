#pragma once

#include "boost/describe/enum.hpp"
namespace AtlasNet
{

enum class WorldSpaceType
{
  Cartesian2D,
  Cartesian3D,
  Geospatial,
  GeospatialAltitude
};
BOOST_DESCRIBE_ENUM(WorldSpaceType, Cartesian2D, Cartesian3D, Geospatial,
                    GeospatialAltitude);

enum class WorldCreationResult
{
  Success,
  AlreadyExists,
  InvalidDefinition,
  DefinitionConflict,
};
BOOST_DESCRIBE_ENUM(WorldCreationResult, Success, AlreadyExists,
                    InvalidDefinition, DefinitionConflict);
} // namespace AtlasNet
