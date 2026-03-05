#pragma once

#include <stop_token>
#include <thread>

#include "Global/Misc/Singleton.hpp"
class HeuristicService : public Singleton<HeuristicService>
{
	std::jthread snapshotThread;

   private:
	void HeuristicThreadLoop(std::stop_token st);


    void ComputeHeuristic();
   public:
	HeuristicService() {}
	};