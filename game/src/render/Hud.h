#pragma once

#include "../core/Math.h"
#include <raylib.h>

namespace fps {

class Player;
struct Weapon;

struct HudState {
    int   botsAlive = 0;
    int   playerKills = 0;
    float matchTime = 0.0f;
    float hitMarkerTimer = 0.0f;
    float damageFlashTimer = 0.0f;
    float killFeedTimer = 0.0f;
};

void drawHud(const HudState& hud, const Player& player, const Weapon& weapon,
             int screenW, int screenH, const char* playerName);

void drawDeathOverlay(int screenW, int screenH, float respawnSec);

} // namespace fps
