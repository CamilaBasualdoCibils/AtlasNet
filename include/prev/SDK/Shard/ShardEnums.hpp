#pragma once

namespace AtlasNet
{
      enum class EntityDetachState
  {
    Begin = 0,  // Freeze entity
    Commit = 1, // Entity is detached and should be removed from the shard's
                // internal tracking and resources
    Cancel = 2, // Unfreeze entity and keep it in the shard's internal tracking
                // and resources
  };
};