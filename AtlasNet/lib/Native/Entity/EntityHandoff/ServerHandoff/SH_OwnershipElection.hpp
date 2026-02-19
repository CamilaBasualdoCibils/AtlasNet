#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "Network/NetworkIdentity.hpp"

class Log;

class SH_OwnershipElection
{
  public:
	struct Options
	{
		std::chrono::seconds evalInterval = std::chrono::seconds(2);
		std::string ownerKey = "EntityHandoff:TestOwnerShard";
	};

	SH_OwnershipElection(const NetworkIdentity& self,
						 std::shared_ptr<Log> inLogger);
	SH_OwnershipElection(const NetworkIdentity& self, std::shared_ptr<Log> inLogger,
						 Options inOptions);

	void Reset(std::chrono::steady_clock::time_point now);
	bool Evaluate(std::chrono::steady_clock::time_point now);
	void Invalidate();
	void ForceNotOwner();

	[[nodiscard]] bool IsOwner() const { return isOwner; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
	bool isOwner = false;
	bool ownershipEvaluated = false;
	bool hasOwnershipLogState = false;
	bool lastOwnershipState = false;
	std::chrono::steady_clock::time_point lastOwnerEvalTime;
};
