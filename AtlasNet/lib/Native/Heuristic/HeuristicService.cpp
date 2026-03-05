#include "HeuristicService.hpp"

#include <thread>
void HeuristicService::HeuristicThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_HEURISTIC_RECOMPUTE_INTERNAL_MS);
	while (!st.stop_requested())
	{
		// Align roughly to system clock
		std::this_thread::sleep_for(interval);
	}
}
