#pragma once

#include <stop_token>
#include <thread>

#include "Global/Misc/Singleton.hpp"
class SnapshotService : public Singleton<SnapshotService>
{
    const char *const SnapshotBoundsID_2_EntityList_Entry_HashTable = "Snapshot::BoundIDs -> EntityList";
	std::jthread snapshotThread;

   public:
	SnapshotService();

   private:
	void SnapshotThreadLoop(std::stop_token st);

	void UploadSnapshot();
};