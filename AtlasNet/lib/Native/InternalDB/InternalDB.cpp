#include "InternalDB.hpp"

#include <cstdlib>
#include <limits>
#include <string>

#include "Database/Redis/Redis.hpp"

namespace
{
std::string ResolveRedisHost()
{
	if (const char* host = std::getenv("INTERNAL_REDIS_SERVICE_NAME");
		host && *host)
	{
		return std::string(host);
	}
	return _INTERNAL_REDIS_SERVICE_NAME;
}

int32_t ResolveRedisPort()
{
	if (const char* portText = std::getenv("INTERNAL_REDIS_PORT");
		portText && *portText)
	{
		char* end = nullptr;
		const long parsed = std::strtol(portText, &end, 10);
		if (end && *end == '\0' && parsed > 0 && parsed <= 65535)
		{
			return static_cast<int32_t>(parsed);
		}
	}
	return _INTERNAL_REDIS_PORT;
}

uint32_t ResolveRedisConnectMaxRetries()
{
	if (const char* retryText = std::getenv("ATLASNET_INTERNALDB_CONNECT_MAX_RETRIES");
		retryText && *retryText)
	{
		char* end = nullptr;
		const unsigned long parsed = std::strtoul(retryText, &end, 10);
		if (end && *end == '\0' && parsed <= std::numeric_limits<uint32_t>::max())
		{
			// Zero means retry forever.
			return static_cast<uint32_t>(parsed);
		}
	}

	// Retry forever by default so pods can survive transient DNS or service recovery windows.
	return 0;
}

uint32_t ResolveRedisConnectRetryIntervalMs()
{
	if (const char* retryText = std::getenv("ATLASNET_INTERNALDB_CONNECT_RETRY_INTERVAL_MS");
		retryText && *retryText)
	{
		char* end = nullptr;
		const unsigned long parsed = std::strtoul(retryText, &end, 10);
		if (end && *end == '\0' && parsed <= std::numeric_limits<uint32_t>::max())
		{
			return static_cast<uint32_t>(parsed);
		}
	}

	// Keep retries fast so a recovered node can rejoin the demo almost immediately.
	return 250;
}
}  // namespace

InternalDB::InternalDB()
{
	redis = Redis::Get().ConnectNonCluster(ResolveRedisHost(), ResolveRedisPort(),
											ResolveRedisConnectMaxRetries(),
											ResolveRedisConnectRetryIntervalMs());
};
