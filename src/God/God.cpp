#include "God.hpp"
#include "pch.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include "curl/curl.h"

God::God() {}
God::~God()
{
    cleanupPartitions();
}

bool God::spawnPartition(int32_t id, int32_t port) 
{
    // Build the shell command to invoke Start.sh
    std::string cmd = "./Start.sh Partition " + std::to_string(id) + " " + std::to_string(port);
    //std::string cmd = "D:/KDNet/KDNet/Start.sh Partition " + std::to_string(id) + " " + std::to_string(port);

    std::cout << "Running: " << cmd << std::endl;

    // Run the script
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Failed to spawn partition " << id << " on port " << port << std::endl;
        return false;
    }

    std::cout << "Partition " << id << " started on port " << port << std::endl;
    return true;
}

bool God::cleanupPartitions() 
{
    int result = std::system("docker rm -f $(docker ps -aq --filter name=partition_)");
    if (result != 0) {
        std::cerr << "Failed to clean up partitions\n" << std::endl;
        return false;
    }
    return true;
}
