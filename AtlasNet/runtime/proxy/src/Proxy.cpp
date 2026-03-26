#include "Proxy.hpp"

#include <boost/uuid/name_generator.hpp>

#include <cstdlib>
#include <thread>

#include "Command/CommandRouter.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Events/GlobalEvents.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"

namespace
{
std::optional<UUID> ResolveStableProxyUUID()
{
	if (const char* configuredUUID = std::getenv("ATLASNET_PROXY_UUID");
		configuredUUID && *configuredUUID)
	{
		try
		{
			return UUIDGen::FromString(configuredUUID);
		}
		catch (...)
		{
		}
	}

	if (const char* podName = std::getenv("POD_NAME"); podName && *podName)
	{
		static const UUID dnsNamespace =
			UUIDGen::FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
		static boost::uuids::name_generator generator(dnsNamespace);
		return generator(std::string("atlasnet-proxy:") + podName);
	}

	return std::nullopt;
}
}  // namespace

void Proxy::Run()
{
	logger->Debug("Init");	// hello

	Init();
	logger->Debug("Loop Entry");
	while (!ShouldShutdown)
	{
		// Interlink::Get().Tick();
		// ProxyLink::Get().Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	logger->Debug("Begin shutdown");
	CleanUp();
	logger->Debug("Shutdown");
}
void Proxy::Init()
{
	CrashHandler::Get().Init();
	const UUID proxyUUID = ResolveStableProxyUUID().value_or(UUIDGen::Gen());
	NetworkCredentials::Make(NetworkIdentity(NetworkIdentityType::eProxy, proxyUUID));

	Interlink::Get().Init();
	CommandRouter::Ensure();
	NetworkManifest::Get().ScheduleNetworkPings();
	HealthManifest::Get().ScheduleHealthPings();
	GlobalEvents::Get().Init();
}
void Proxy::Shutdown()
{
	ShouldShutdown = true;
}
void Proxy::CleanUp()
{
	// Interlink::Get().Shutdown();
}
bool Proxy::OnAcceptConnection(const Connection& c)
{
	return true;
}
void Proxy::OnConnected(const NetworkIdentity& id) {}
void Proxy::OnDisconnected(const NetworkIdentity& id) {}
void Proxy::OnMessageReceived(const Connection& from, std::span<const std::byte> data) {}
