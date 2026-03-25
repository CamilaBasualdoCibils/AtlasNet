#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "StreamEntityLedgerEntry.hpp"

class StreamEntityLedgersView
{
	using Clock = std::chrono::steady_clock;

	struct ShardLedgerState
	{
		std::vector<StreamEntityLedgerEntry> entries;
		std::optional<BoundsID> boundId;
		Clock::time_point lastRequestAt = Clock::time_point::min();
		Clock::time_point lastResponseAt = Clock::time_point::min();
		Clock::time_point lastTimeoutLogAt = Clock::time_point::min();
		bool awaitingResponse = false;
	};

	static constexpr auto kRefreshLoopSleep = std::chrono::milliseconds(5);
	static constexpr auto kRequestInterval = std::chrono::milliseconds(5);
	static constexpr auto kRequestTimeout = std::chrono::milliseconds(250);
	static constexpr auto kSnapshotTTL = std::chrono::milliseconds(5000);
	static constexpr auto kTimeoutLogInterval = std::chrono::seconds(5);

	Log logger = Log("StreamEntityLedgersView");
	PacketManager::Subscription subToLocalEntityListRequestPacket;
	std::mutex stateMutex;
	std::unordered_map<NetworkIdentity, ShardLedgerState> shardStates;
	std::jthread refreshThread;
	Clock::time_point lastDataFailureLogAt = Clock::time_point::min();

	void LogDataFailureThrottled(std::string_view message)
	{
		const auto now = Clock::now();
		if (lastDataFailureLogAt == Clock::time_point::min() ||
			(now - lastDataFailureLogAt) >= kTimeoutLogInterval)
		{
			logger.Warning(message.data());
			lastDataFailureLogAt = now;
		}
	}

	void OnLocalEntityListRequestPacket(const LocalEntityListRequestPacket& p,
										const PacketManager::PacketInfo& info)
	{
		if (p.status != LocalEntityListRequestPacket::MsgStatus::eResponse)
		{
			return;
		}

		std::vector<StreamEntityLedgerEntry> shardEntries;
		shardEntries.reserve(std::get<std::vector<AtlasEntityMinimal>>(p.Response_Entities).size());

		std::optional<BoundsID> boundId;
		try
		{
			boundId = HeuristicManifest::Get().QueryOwnershipState(
				[&](const HeuristicManifest::OwnershipStateWrapper& w)
				{ return w.GetBoundOwner(info.sender); });
		}
		catch (const std::exception&)
		{
			LogDataFailureThrottled(
				"Entity ledger stream ownership lookup failed; falling back to cached shard state.");
		}
		catch (...)
		{
			LogDataFailureThrottled(
				"Entity ledger stream ownership lookup failed; falling back to cached shard state.");
		}

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			auto& shardState = shardStates[info.sender];
			if (!boundId.has_value())
			{
				boundId = shardState.boundId;
			}
			else
			{
				shardState.boundId = boundId;
			}

			if (!boundId.has_value())
			{
				shardState.awaitingResponse = false;
				logger.WarningFormatted(
					"LocalEntityListRequestPacket arrived but bound ID could not be determined. {}",
					info.sender.ToString());
				return;
			}

			for (const auto& ae : std::get<std::vector<AtlasEntityMinimal>>(p.Response_Entities))
			{
				StreamEntityLedgerEntry entry;
				entry.BoundID = *boundId;
				entry.ClientID = UUIDGen::ToString(ae.Client_ID);
				entry.EntityID = UUIDGen::ToString(ae.Entity_ID);
				entry.ISClient = ae.IsClient;
				entry.positionx = ae.transform.position.x;
				entry.positiony = ae.transform.position.y;
				entry.positionz = ae.transform.position.z;
				entry.WorldID = 0;
				shardEntries.push_back(std::move(entry));
			}

			shardState.entries = std::move(shardEntries);
			shardState.awaitingResponse = false;
			shardState.lastResponseAt = Clock::now();
		}
	}

	void RefreshShardRequests()
	{
		const auto now = Clock::now();
		std::unordered_set<NetworkIdentity> liveShards;
		std::vector<NetworkIdentity> shardsToQuery;
		bool liveShardRefreshSucceeded = false;

		try
		{
			std::vector<std::string> liveShardKeys;
			HealthManifest::Get().GetLivePings(liveShardKeys);
			for (const auto& liveShardKey : liveShardKeys)
			{
				ByteReader br(liveShardKey);
				NetworkIdentity netID;
				netID.Deserialize(br);
				if (netID.Type == NetworkIdentityType::eShard)
				{
					liveShards.insert(netID);
				}
			}
			liveShardRefreshSucceeded = true;
		}
		catch (const std::exception&)
		{
			LogDataFailureThrottled(
				"Entity ledger stream failed to refresh live shards from the database; keeping current shard set.");
		}
		catch (...)
		{
			LogDataFailureThrottled(
				"Entity ledger stream failed to refresh live shards from the database; keeping current shard set.");
		}

		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (!liveShardRefreshSucceeded)
			{
				for (const auto& [shardId, shardState] : shardStates)
				{
					(void)shardState;
					liveShards.insert(shardId);
				}
			}

			std::erase_if(
				shardStates,
				[&liveShards](const auto& item) { return !liveShards.contains(item.first); });

			for (const auto& shardId : liveShards)
			{
				auto& shardState = shardStates[shardId];
				if (shardState.awaitingResponse)
				{
					if ((now - shardState.lastRequestAt) < kRequestTimeout)
					{
						continue;
					}

					shardState.awaitingResponse = false;
					if (shardState.lastTimeoutLogAt == Clock::time_point::min() ||
						(now - shardState.lastTimeoutLogAt) >= kTimeoutLogInterval)
					{
						logger.WarningFormatted(
							"Shard {} did not respond to entity ledger stream refresh within {}ms.",
							shardId.ToString(), kRequestTimeout.count());
						shardState.lastTimeoutLogAt = now;
					}
				}

				if (shardState.lastRequestAt != Clock::time_point::min() &&
					(now - shardState.lastRequestAt) < kRequestInterval)
				{
					continue;
				}

				shardState.awaitingResponse = true;
				shardState.lastRequestAt = now;
				shardsToQuery.push_back(shardId);
			}
		}

		for (const auto& shardId : shardsToQuery)
		{
			LocalEntityListRequestPacket packet;
			packet.status = LocalEntityListRequestPacket::MsgStatus::eQuery;
			packet.Request_IncludeMetadata = false;
			Interlink::Get().SendMessage(shardId, packet, NetworkMessageSendFlag::eUnreliableNow);
		}
	}

	void RefreshLoop(std::stop_token stopToken)
	{
		while (!stopToken.stop_requested())
		{
			try
			{
				RefreshShardRequests();
			}
			catch (const std::exception&)
			{
				LogDataFailureThrottled(
					"Entity ledger stream refresh loop hit an unexpected error; retrying.");
			}
			catch (...)
			{
				LogDataFailureThrottled(
					"Entity ledger stream refresh loop hit an unexpected error; retrying.");
			}
			std::this_thread::sleep_for(kRefreshLoopSleep);
		}
	}

   public:
	StreamEntityLedgersView()
	{
		subToLocalEntityListRequestPacket =
			Interlink::Get().GetPacketManager().Subscribe<LocalEntityListRequestPacket>(
				[this](const auto& p, const auto& info)
				{ OnLocalEntityListRequestPacket(p, info); });
		refreshThread = std::jthread([this](std::stop_token st) { RefreshLoop(st); });
	}

	~StreamEntityLedgersView() = default;

	void GetEntityLists(std::vector<StreamEntityLedgerEntry>& entities,
						std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
	{
		(void)timeout;

		const auto now = Clock::now();
		std::lock_guard<std::mutex> lock(stateMutex);

		entities.clear();
		for (const auto& [shardId, shardState] : shardStates)
		{
			(void)shardId;
			if (shardState.lastResponseAt == Clock::time_point::min())
			{
				continue;
			}
			if ((now - shardState.lastResponseAt) > kSnapshotTTL)
			{
				continue;
			}

			entities.insert(entities.end(), shardState.entries.begin(), shardState.entries.end());
		}

		logger.DebugFormatted("GetEntityLists returned {} streamed entries", entities.size());
	}
};
