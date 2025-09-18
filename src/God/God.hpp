#pragma once
#include "pch.hpp"
#include "Singleton.hpp"

class God : public Singleton<God> {
   public:
    God();
    ~God();

    bool spawnPartition(int32 id, int32 port);
    bool removePartition(int32 id);
    bool cleanupPartitions();

    std::set<int32> getPartitionIDs();

    // returns null for now
    void* getPartition(int32 id);

    private:
    // hold all ids
    std::set<int32> partitionIds;

    //in case of SIGINT or SIGTERM (doesnt work atm. cannot catch SIGs)
    static void handleSignal(int32 signum);
};