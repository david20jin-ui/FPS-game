#include "core/App.h"
#include "core/Cli.h"

#include <cstdio>

int main(int argc, char** argv) {
    fps::Config cfg = fps::parseCli(argc, argv);
    std::printf("[fps_game] starting match=%s server=%s:%d token=%s name=%s fov=%.1f sens=%.2f\n",
                cfg.mode.c_str(), cfg.serverIp.c_str(), cfg.serverPort,
                cfg.token.c_str(), cfg.playerName.c_str(),
                cfg.fovDegrees, cfg.sensitivity);
    return fps::runApp(cfg);
}
