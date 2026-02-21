#pragma once

#include <chrono>
#include <stop_token>
#include <thread>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IBounds.hpp"
#include "Network/NetworkIdentity.hpp"
class BoundLeaser : public Singleton<BoundLeaser>
{
	Log logger = Log("BoundLeaser");
	std::unique_ptr<IBounds> ClaimedBound;
	IBounds::BoundsID ClaimedBoundID;
	NetworkIdentity ID;

	std::jthread LoopThread;

   private:
	void LoopEntry(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			if (!ClaimedBound)
			{
				ClaimBound();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void ClaimBound();

   public:
	void Init(const NetworkIdentity& _ID)
	{
		ID = _ID;
		logger.Debug("Init");
		LoopThread = std::jthread([this](std::stop_token st) { LoopEntry(st); });
	}
};
