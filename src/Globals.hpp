#pragma once
#include "pch.hpp"

template <typename T>
T Env2Var(const std::string &variable) {
    const char *env = std::getenv(variable.c_str());
    std::cout << "Fetching " << variable << std::endl;
    if (!env) {
	throw std::runtime_error("Environment variable not found: " + variable);
    }

    if constexpr (std::is_same_v<T, std::string>) {
	return std::string(env);
    } else if constexpr (std::is_same_v<T, int32>) {
	return std::stoi(env);
    } else if constexpr (std::is_same_v<T, int64>) {
	return std::stoll(env);
    } else if constexpr (std::is_same_v<T, float>) {
	return std::stof(env);
    } else if constexpr (std::is_same_v<T, double>) {
	return std::stod(env);
    } else {
	static_assert(!sizeof(T *), "Unsupported type for Env2Var");
    }
};
using PartitionID = uint32;

//const static inline int32 InComposePartitionPort = Env2Var<int32>("IN_COMPOSE_PARTITION_PORT");