#include "InternalDB.hpp"

#include <cstdlib>
#include <limits>
#include <string>

#include "Database/Redis/Redis.hpp"

namespace
{
uint32_t ResolveRedisUnsignedEnv(const char* name, const uint32_t fallback)
{
	if (const char* valueText = std::getenv(name); valueText && *valueText)
	{
		char* end = nullptr;
		const unsigned long parsed = std::strtoul(valueText, &end, 10);
		if (end && *end == '\0' && parsed <= std::numeric_limits<uint32_t>::max())
		{
			return static_cast<uint32_t>(parsed);
		}
	}
	return fallback;
}

std::string ResolveRedisHost()
{
	if (const char* serviceHost = std::getenv("INTERNALDB_SERVICE_HOST");
		serviceHost && *serviceHost)
	{
		return std::string(serviceHost);
	}
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

uint32_t ResolveRedisConnectRetries()
{
	return ResolveRedisUnsignedEnv("INTERNAL_REDIS_CONNECT_RETRIES", 20);
}

uint32_t ResolveRedisRetryIntervalMs()
{
	return ResolveRedisUnsignedEnv("INTERNAL_REDIS_RETRY_INTERVAL_MS", 500);
}

uint32_t ResolveRedisConnectTimeoutMs()
{
	return ResolveRedisUnsignedEnv("INTERNAL_REDIS_CONNECT_TIMEOUT_MS", 250);
}

uint32_t ResolveRedisSocketTimeoutMs()
{
	return ResolveRedisUnsignedEnv("INTERNAL_REDIS_SOCKET_TIMEOUT_MS", 1000);
}
}  // namespace

InternalDB::InternalDB()
{
	redis = Redis::Get().ConnectNonCluster(ResolveRedisHost(), ResolveRedisPort(),
											   ResolveRedisConnectRetries(),
											   ResolveRedisRetryIntervalMs(),
											   ResolveRedisConnectTimeoutMs(),
											   ResolveRedisSocketTimeoutMs());
};
