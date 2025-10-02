#pragma once
#include <cstdlib>                // for std::system
#include <iostream>
#include "pch.hpp"

/**
 * Starts a Redis Docker container if not already running,
 * then returns a connected Redis client.
 *
 * @param containerName Name of the container (e.g. "database-redis")
 * @param port          Host port to map Redis on (default 6379)
 */
int32 StartDatabaseRedis(const std::string &containerName = "database-redis",
                                      int32 port = 6379) {
          std::string cmd =
        "docker rm -f " + containerName + " >/dev/null 2>&1; "
        "docker run --network AtlasNet -d --name " + containerName + " -p " + std::to_string(port) + ":6379 redis:latest >/dev/null";

            int32 ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "❌ Failed to start Redis container\n";
    }
    return ret;
}
