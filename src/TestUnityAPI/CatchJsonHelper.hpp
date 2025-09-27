// KDNetReceiver.hpp

#pragma once
#include <string>
#include <cstdio>
#include <array>
#include <stdexcept>

#include <nlohmann/json.hpp>
using nlohmann::json;

namespace UnityTest {

struct Entity {
    std::string id;
    std::array<float,3> position{0,0,0};
};

enum Status {
    OK = 0,
    ERR_PARSE = 1,
    ERR_SCHEMA = 2
};

/// Parse KDNet entity JSON into `Entity`.
/// @param body JSON string payload
/// @param out  Filled on success
/// @param err  Optional error description
/// @return true on success
inline bool TryParseEntity(const std::string& body, Entity& out, std::string* err = nullptr) {
    try {
        json j = json::parse(body);

        // Validate required fields
        if (!j.contains("id") || !j["id"].is_string()) {
            if (err) *err = "missing or invalid 'id'";
            return false;
        }
        if (!j.contains("position") || !j["position"].is_array() || j["position"].size() != 3) {
            if (err) *err = "missing or invalid 'position' (expected 3 floats)";
            return false;
        }

        out.id = j["id"].get<std::string>();
        for (size_t i = 0; i < 3; ++i) {
            // nlohmann will convert numeric types safely
            out.position[i] = j["position"][i].get<float>();
        }

        return true;
    } catch (const std::exception& ex) {
        if (err) *err = std::string("json parse error: ") + ex.what();
        return false;
    }
}

/// Convenience: parse and print a one-liner. Returns KDNet::Status.
inline int PrintEntityFromJson(const std::string& body) {
    Entity e;
    std::string err;
    if (!TryParseEntity(body, e, &err)) {
        std::fprintf(stderr, "[KDNet Server] parse failed: %s | payload: %s\n",
                     err.c_str(), body.c_str());
        return ERR_PARSE;
    }
    std::printf("[KDNet Server] id=%s pos=(%.3f, %.3f, %.3f)\n",
                e.id.c_str(), e.position[0], e.position[1], e.position[2]);
    return OK;
}

/// Pretty printer if you already parsed.
inline void Print(const Entity& e) {
    std::printf("[KDNet Server] id=%s pos=(%.3f, %.3f, %.3f)\n",
                e.id.c_str(), e.position[0], e.position[1], e.position[2]);
}

} // namespace KDNet
