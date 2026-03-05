#include "SnapshotService.hpp"

#include <boost/container/static_vector.hpp>
#include <charconv>
#include <chrono>
#include <cstring>
#include <execution>
#include <string>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/IBounds.hpp"
#include "InternalDB/InternalDB.hpp"
void SnapshotService::SnapshotThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_SNAPSHOT_INTERNAL_MS);
	while (!st.stop_requested())
	{
		// Align roughly to system clock
		auto now = steady_clock::now();
		auto now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

		// Compute time until next "tick" of interval
		auto next_tick_ms = ((now_ms.count() / interval.count()) + 1) * interval.count();
		auto sleep_duration = milliseconds(next_tick_ms - now_ms.count());

		// Add tiny random jitter to avoid perfect collisions
		sleep_duration += milliseconds(rand() % 10);  // 0-9 ms jitter

		// Sleep until next tick
		if (sleep_duration > milliseconds(0))
			std::this_thread::sleep_for(sleep_duration);

		if (st.stop_requested())
			break;

		UploadSnapshot();
	}
}
SnapshotService::SnapshotService()
{
	snapshotThread = std::jthread([this](std::stop_token st) { SnapshotThreadLoop(st); });
};

void SnapshotService::UploadSnapshot()
{
	if (BoundLeaser::Get().HasBound())
	{
		IBounds::BoundsID boundID = BoundLeaser::Get().GetBoundID();
		ByteWriter entityListWriter;
		EntityLedger::Get().ForEachEntityRead(std::execution::seq, [&](const AtlasEntity& entity)
											  { entity.Serialize(entityListWriter); });
		boost::container::static_vector<char, 32> boundString;
		boundString.resize(32);
		std::to_chars(boundString.data(), boundString.data() + boundString.size(), boundID);
		InternalDB::Get()->HSet(SnapshotBoundsID_2_EntityList_Entry_HashTable, boundString.data(),
								entityListWriter.as_string_view());
	}
	else
	{
	}
}
