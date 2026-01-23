#pragma once
#include <chrono>
#include <thread>

#include "Misc/Singleton.hpp"
#include "InterlinkIdentifier.hpp"
#include "Interlink.hpp"
#include "InternalDB.hpp"
#include "ConnectionTelemetry.hpp"
#include "Serialize/ByteWriter.hpp"



class NetworkManifest : public Singleton<NetworkManifest> {
public:

    const std::string NetworkTelemetryTable = "Network_Telemetry";

	std::optional<InterLinkIdentifier> identifier;
	std::jthread HealthPingIntervalFunc;

	std::jthread HealthCheckOnFailureFunc;

public:
	void ScheduleNetworkPings(const InterLinkIdentifier& id)
	{
        //
		HealthPingIntervalFunc = std::jthread(
			[ id = id](std::stop_token st)
			{
				while (!st.stop_requested())
				{
					NetworkManifest::Get().TelemetryUpdate(id);
					std::this_thread::sleep_for(
						std::chrono::milliseconds(_NETWORK_TELEMETRY_PING_INTERVAL_MS));
				}
			});
	}

    void TelemetryUpdate(const InterLinkIdentifier& identifier)
	{
        std::string_view s_b, s_id;
        // set data from interlink
        std::vector<ConnectionTelemetry> out;
        Interlink::Get().GetConnectionTelemetry(out);
		ByteWriter bw;
        for (const auto& t : out) {
            t.Serialize(bw);
            std::cerr << t.targetId << std::endl;
        }
        s_b = bw.as_string_view();

        ByteWriter bw_id;
		bw_id.str(identifier.ID);
		s_id = bw_id.as_string_view();

        // write to internal db
        // needs to overwrite val using identity + target (concat?) as name
		const auto setResult = InternalDB::Get()->HSet(NetworkTelemetryTable, s_id, s_b);
		if (setResult != 0)
		{
			std::printf("Failed to update network telemetry in Network Manifest. HSET result: %lli",setResult);
		}
	
	}
};

