#include "Cli.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace fps {

namespace {

void applyServer(Config& cfg, std::string_view value) {
    auto colon = value.find(':');
    if (colon == std::string_view::npos) {
        cfg.serverIp = std::string(value);
        return;
    }
    cfg.serverIp   = std::string(value.substr(0, colon));
    cfg.serverPort = std::atoi(std::string(value.substr(colon + 1)).c_str());
}

} // namespace

Config parseCli(int argc, char** argv) {
    Config cfg{};
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg.rfind("--", 0) != 0) continue;
        arg.remove_prefix(2);
        auto eq = arg.find('=');
        std::string_view key   = eq == std::string_view::npos ? arg : arg.substr(0, eq);
        std::string_view value = eq == std::string_view::npos ? std::string_view{} : arg.substr(eq + 1);

        if (key == "server") {
            applyServer(cfg, value);
        } else if (key == "token") {
            cfg.token = std::string(value);
        } else if (key == "name") {
            cfg.playerName = std::string(value);
        } else if (key == "mode") {
            cfg.mode = std::string(value);
        } else if (key == "fov") {
            cfg.fovDegrees = static_cast<float>(std::atof(std::string(value).c_str()));
        } else if (key == "sensitivity") {
            cfg.sensitivity = static_cast<float>(std::atof(std::string(value).c_str()));
        } else if (key == "width") {
            cfg.windowWidth = std::atoi(std::string(value).c_str());
        } else if (key == "height") {
            cfg.windowHeight = std::atoi(std::string(value).c_str());
        } else if (key == "fullscreen") {
            cfg.fullscreen = value == "1" || value == "true";
        } else if (key == "help") {
            std::printf(
                "fps_game CLI:\n"
                "  --server=<ip>:<port>     matchmaker-provided server (default 127.0.0.1:7777)\n"
                "  --token=<token>          opaque match token\n"
                "  --name=<name>            display name\n"
                "  --mode=solo|team         match mode (default solo)\n"
                "  --fov=<deg>              field of view (default 90)\n"
                "  --sensitivity=<f>        mouse sensitivity (default 0.4)\n"
                "  --width=<px> --height=<px>   window size\n"
                "  --fullscreen=1           run fullscreen\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "[cli] unknown flag: --%.*s\n", static_cast<int>(key.size()), key.data());
        }
    }
    return cfg;
}

} // namespace fps
