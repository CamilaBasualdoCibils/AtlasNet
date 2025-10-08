#pragma once
#include <string>
#include <optional>
#include <iostream>
#include <cstdlib>
#include "pch.hpp"
#include "IDatabase.hpp"

class RedisCacheDatabase : public IDatabase {
public:
    RedisCacheDatabase(bool createDatabase = false, const std::string &host = "database-redis", int32 port = 6379, const std::string &network = "AtlasNet");

    // --- Public interface ---
    /**
     * Initializes connection to Redis.
     * If autoStart==true, will spin up database 
     */
    bool Connect() override;


    bool Set(const std::string &key, const std::string &value) override;

    std::string Get(const std::string &key) override;

    bool Remove(const std::string &key) override;

    bool Exists(const std::string &key) override;

    void PrintEntireDB() override;

private:
    std::string _host;
    int32 _port;
    std::string _network;
    bool _autoStart;

    std::unique_ptr<sw::redis::Redis> _redis;
};
