#include "Hud.h"

#include "../player/Player.h"
#include "../weapons/Weapon.h"

#include <cstdio>

namespace fps {

void drawHud(const HudState& hud, const Player& player, const Weapon& weapon,
             int screenW, int screenH, const char* playerName) {
    if (hud.damageFlashTimer > 0.0f) {
        unsigned char a = static_cast<unsigned char>(std::min(180.0f, hud.damageFlashTimer * 300.0f));
        DrawRectangle(0, 0, screenW, screenH, Color{255, 40, 60, a});
    }

    int cx = screenW / 2;
    int cy = screenH / 2;
    const int gap = 4;
    const int len = 6;
    const int thick = 2;
    Color ch{220, 235, 255, 230};
    float horizSpeed = v3Len(Vector3{player.velocity.x, 0, player.velocity.z});
    int mg = gap + static_cast<int>(clampf(horizSpeed * 1.2f, 0.0f, 14.0f));
    DrawRectangle(cx - mg - len, cy - thick / 2, len, thick, ch);
    DrawRectangle(cx + mg,        cy - thick / 2, len, thick, ch);
    DrawRectangle(cx - thick / 2, cy - mg - len,  thick, len, ch);
    DrawRectangle(cx - thick / 2, cy + mg,        thick, len, ch);
    DrawCircle(cx, cy, 1.2f, ch);

    if (hud.hitMarkerTimer > 0.0f) {
        Color hm{255, 100, 100, 230};
        int off = 12;
        int ln = 6;
        DrawLineEx({(float)cx - off, (float)cy - off}, {(float)cx - off + ln, (float)cy - off + ln}, 2, hm);
        DrawLineEx({(float)cx + off, (float)cy - off}, {(float)cx + off - ln, (float)cy - off + ln}, 2, hm);
        DrawLineEx({(float)cx - off, (float)cy + off}, {(float)cx - off + ln, (float)cy + off - ln}, 2, hm);
        DrawLineEx({(float)cx + off, (float)cy + off}, {(float)cx + off - ln, (float)cy + off - ln}, 2, hm);
    }

    int mins = static_cast<int>(hud.matchTime) / 60;
    int secs = static_cast<int>(hud.matchTime) % 60;
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);
    int tw = MeasureText(timeBuf, 28);
    DrawRectangle(cx - tw / 2 - 12, 16, tw + 24, 36, Color{0, 0, 0, 140});
    DrawText(timeBuf, cx - tw / 2, 20, 28, Color{230, 235, 255, 255});

    char scoreBuf[64];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "%s    Kills %d    Bots %d",
                  playerName, hud.playerKills, hud.botsAlive);
    DrawRectangle(16, 16, MeasureText(scoreBuf, 18) + 24, 34, Color{0, 0, 0, 140});
    DrawText(scoreBuf, 28, 23, 18, Color{230, 235, 255, 255});

    int hx = 28;
    int hy = screenH - 80;
    DrawRectangle(hx - 12, hy - 10, 260, 68, Color{0, 0, 0, 160});
    char healthBuf[16];
    std::snprintf(healthBuf, sizeof(healthBuf), "%d", player.health);
    DrawText(healthBuf, hx, hy, 32, Color{230, 235, 255, 255});
    float frac = clampf(static_cast<float>(player.health) / 100.0f, 0.0f, 1.0f);
    DrawRectangle(hx + 60, hy + 6, 160, 10, Color{40, 50, 70, 255});
    DrawRectangle(hx + 60, hy + 6, static_cast<int>(160 * frac), 10, Color{80, 220, 130, 255});
    char armorBuf[16];
    std::snprintf(armorBuf, sizeof(armorBuf), "ARMOR %d", player.armor);
    DrawText(armorBuf, hx + 60, hy + 22, 16, Color{180, 210, 240, 255});

    int ax = screenW - 220;
    int ay = screenH - 80;
    DrawRectangle(ax - 12, ay - 10, 210, 68, Color{0, 0, 0, 160});
    DrawText(weapon.stats.name.c_str(), ax, ay, 18, Color{200, 210, 230, 255});
    char ammoBuf[32];
    if (weapon.stats.kind == WeaponKind::Knife) {
        std::snprintf(ammoBuf, sizeof(ammoBuf), "KNIFE");
    } else {
        std::snprintf(ammoBuf, sizeof(ammoBuf), "%d / %d",
                      weapon.ammoInMag, weapon.stats.magSize);
    }
    DrawText(ammoBuf, ax, ay + 22, 28, Color{230, 235, 255, 255});
    if (weapon.reloadLeft > 0.0f) {
        float f = 1.0f - (weapon.reloadLeft / weapon.stats.reloadTime);
        DrawRectangle(ax, ay + 54, 180, 6, Color{40, 50, 70, 255});
        DrawRectangle(ax, ay + 54, static_cast<int>(180 * f), 6, Color{255, 180, 80, 255});
    }

    const char* hints = "WASD move  SHIFT walk  CTRL crouch  SPACE jump  LMB fire  R reload  1 Vandal 2 Phantom 3 Spectre 4 Operator 5 Classic 6 Knife  G smoke";
    int hw = MeasureText(hints, 14);
    DrawText(hints, cx - hw / 2, screenH - 22, 14, Color{140, 150, 170, 230});
}

void drawDeathOverlay(int screenW, int screenH, float respawnSec) {
    DrawRectangle(0, 0, screenW, screenH, Color{10, 10, 15, 180});
    const char* msg = "YOU DIED";
    int w = MeasureText(msg, 56);
    DrawText(msg, screenW / 2 - w / 2, screenH / 2 - 80, 56, Color{255, 80, 90, 255});
    char b[64];
    std::snprintf(b, sizeof(b), "Respawning in %.1f s", respawnSec);
    int bw = MeasureText(b, 22);
    DrawText(b, screenW / 2 - bw / 2, screenH / 2 + 10, 22, Color{230, 235, 255, 255});
}

} // namespace fps
