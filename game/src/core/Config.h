#pragma once

#include <string>

namespace fps {

struct Config {
    std::string serverIp   = "127.0.0.1";
    int         serverPort = 7777;
    std::string token      = "dev";
    std::string playerName = "Agent";
    std::string mode       = "solo";
    float       fovDegrees = 90.0f;
    float       sensitivity = 0.4f;
    int         windowWidth  = 1600;
    int         windowHeight = 900;
    bool        fullscreen   = false;
};

} // namespace fps
