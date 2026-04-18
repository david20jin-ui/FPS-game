#include "App.h"

#include "../ai/Bot.h"
#include "../net/NetClient.h"
#include "../player/Player.h"
#include "../render/Hud.h"
#include "../render/SoundBank.h"
#include "../render/ViewModel.h"
#include "../weapons/Smoke.h"
#include "../weapons/Weapon.h"
#include "../world/Map.h"
#include "Math.h"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace fps {

namespace {

struct TracerEffect {
    Vector3 from{};
    Vector3 to{};
    float life = 0.0f;
    Color  color = WHITE;
};

struct MuzzleFlashEffect {
    float life = 0.0f;
};

// Latest-known state for a non-local entity (remote player or server-owned bot).
struct RemoteEntity {
    uint8_t id = 0;
    uint8_t kind = 0;    // 0 = player, 1 = bot
    uint8_t team = 2;    // 0 blue, 1 red, 2 unaffiliated
    bool    alive = true;
    int     health = 100;
    int     kills = 0;
    Vector3 position{};
    float   yaw = 0.0f;
    float   pitch = 0.0f;
};

Color teamColor(uint8_t team) {
    switch (team) {
    case 0: return Color{ 80, 150, 230, 255}; // blue
    case 1: return Color{230,  80,  90, 255}; // red
    default: return Color{200, 200, 200, 255};
    }
}

Color teamColorDark(uint8_t team) {
    switch (team) {
    case 0: return Color{ 40,  80, 140, 255};
    case 1: return Color{140,  40,  50, 255};
    default: return Color{ 90,  90,  90, 255};
    }
}

void drawRemoteAgent(const RemoteEntity& e) {
    if (!e.alive) return;
    const float r = 0.35f, h = 1.7f;
    Vector3 body{e.position.x, e.position.y + h * 0.55f, e.position.z};
    Vector3 head{e.position.x, e.position.y + h - 0.15f, e.position.z};
    DrawCube(body, r * 1.8f, h, r * 1.8f, teamColor(e.team));
    DrawCubeWires(body, r * 1.8f, h, r * 1.8f, teamColorDark(e.team));
    DrawSphere(head, r * 0.9f, teamColor(e.team));
    Vector3 aim{head.x + std::sin(e.yaw) * 0.5f,
                head.y + std::sin(e.pitch) * 0.5f,
                head.z + std::cos(e.yaw) * 0.5f};
    DrawLine3D(head, aim, Color{255, 220, 80, 255});
    // Floating health bar.
    float frac = clampf(static_cast<float>(e.health) / 100.0f, 0.0f, 1.0f);
    Vector3 barPos{e.position.x, e.position.y + h + 0.25f, e.position.z};
    DrawCube(barPos, 0.6f, 0.07f, 0.02f, Color{0, 0, 0, 180});
    DrawCube({barPos.x - 0.30f + 0.30f * frac, barPos.y, barPos.z},
             0.6f * frac, 0.06f, 0.03f, Color{60, 220, 110, 255});
}

void drawBot(const Bot& bot) {
    if (!bot.alive) return;
    Vector3 body{bot.position.x, bot.position.y + bot.height() * 0.55f, bot.position.z};
    Vector3 head{bot.position.x, bot.position.y + bot.height() - 0.15f, bot.position.z};
    DrawCube(body, bot.radius() * 1.8f, bot.height() * 1.0f, bot.radius() * 1.8f,
             Color{200, 80, 80, 255});
    DrawCubeWires(body, bot.radius() * 1.8f, bot.height() * 1.0f, bot.radius() * 1.8f,
                  Color{120, 30, 30, 255});
    DrawSphere(head, bot.radius() * 0.9f, Color{230, 120, 120, 255});

    Vector3 aim{head.x + std::sin(bot.yaw) * 0.5f,
                head.y + std::sin(bot.pitch) * 0.5f,
                head.z + std::cos(bot.yaw) * 0.5f};
    DrawLine3D(head, aim, Color{255, 220, 80, 255});
}

bool rayHitsBot(Vector3 origin, Vector3 dir, float maxDist, const Bot& bot,
                float& outT, bool& outHeadshot) {
    if (!bot.alive) return false;
    const float r = bot.radius();
    const float yBottom = bot.position.y;
    const float yTop    = bot.position.y + bot.height();

    Vector3 oc{origin.x - bot.position.x, 0.0f, origin.z - bot.position.z};
    Vector3 d {dir.x, 0.0f, dir.z};
    float A = d.x * d.x + d.z * d.z;
    float B = 2.0f * (oc.x * d.x + oc.z * d.z);
    float C = oc.x * oc.x + oc.z * oc.z - r * r;
    if (A < 1e-6f) return false;
    float disc = B * B - 4 * A * C;
    if (disc < 0) return false;
    float sq = std::sqrt(disc);
    float t1 = (-B - sq) / (2 * A);
    float t2 = (-B + sq) / (2 * A);
    float t = t1 >= 0 ? t1 : t2;
    if (t < 0 || t > maxDist) return false;

    float hitY = origin.y + dir.y * t;
    if (hitY < yBottom || hitY > yTop) return false;

    outT = t;
    float headCutoff = yTop - bot.height() * 0.12f;
    outHeadshot = hitY >= headCutoff;
    return true;
}

} // namespace

int runApp(const Config& cfg) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(cfg.windowWidth, cfg.windowHeight, "FPS // Valorant-Inspired");
    if (cfg.fullscreen) ToggleFullscreen();
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);
    DisableCursor();

    InitAudioDevice();
    SoundBank sfx;
    sfx.load();

    Map world = Map::buildByName(cfg.mapName);

    // Translate CLI difficulty string to per-bot BotDifficulty.
    auto parseDifficulty = [](const std::string& s) {
        if (s == "easy")   return BotDifficulty::Easy;
        if (s == "hard")   return BotDifficulty::Hard;
        return BotDifficulty::Normal;
    };
    BotDifficulty selectedDifficulty = parseDifficulty(cfg.difficulty);

    // Currently-selected map and difficulty labels (mutable because the
    // in-game main menu lets the player cycle through them).
    std::string currentMapName    = cfg.mapName;
    std::string currentDifficulty = cfg.difficulty;

    Player player{};
    player.spawn(world.playerSpawn);
    player.fovDegrees = cfg.fovDegrees;
    player.sensitivity = cfg.sensitivity;

    std::vector<Weapon> loadout = {
        Weapon::makeVandal(),
        Weapon::makePhantom(),
        Weapon::makeSpectre(),
        Weapon::makeOperator(),
        Weapon::makeClassic(),
        Weapon::makeKnife(),
    };
    Weapon smokeUtility = Weapon::makeSmoke();
    int currentWeapon = 0;

    std::vector<Bot> bots;
    int bId = 0;
    auto spawnBots = [&]() {
        bots.clear();
        for (size_t i = 0; i < world.botSpawns.size(); ++i) {
            Bot b{};
            b.id = bId++;
            b.position = world.botSpawns[i];
            b.difficulty = selectedDifficulty;
            switch (selectedDifficulty) {
            case BotDifficulty::Easy:   b.weapon = Weapon::makeClassic(); break;
            case BotDifficulty::Normal: b.weapon = Weapon::makeSpectre(); break;
            case BotDifficulty::Hard:   b.weapon = Weapon::makeVandal();  break;
            }
            bots.push_back(b);
        }
    };
    spawnBots();

    std::vector<SmokeProjectile> smokeProjs;
    std::vector<SmokeVolume>     smokeVols;
    std::vector<TracerEffect>    tracers;
    MuzzleFlashEffect muzzle{};
    ViewModelState    vm{};

    // Networking state (connection is established further down after `hud`
    // is declared, because the callbacks reference it by reference).
    const bool wantNet = (cfg.mode == "team");
    std::unique_ptr<fps::net::NetClient> netClient;
    bool netConnected = false;
    uint8_t myClientId = 0;
    uint8_t myTeam = 0;
    uint8_t lastScoreBlue = 0, lastScoreRed = 0;
    uint32_t nextInputSeq = 0;
    std::vector<RemoteEntity> remoteEntities;
    Vector3 lastAuthorityPos = player.position;

    HudState hud{};
    hud.botsAlive = static_cast<int>(bots.size());

    // ----- Attempt the network handshake ---------------------------------
    // Team mode connects to the dedicated `fps_server`; solo stays local.
    if (wantNet) {
        if (fps::net::NetClient::globalInit()) {
            netClient = std::make_unique<fps::net::NetClient>();
            if (netClient->connect(cfg.serverIp,
                                   static_cast<uint16_t>(cfg.serverPort),
                                   cfg.playerName, 5000)) {
                netConnected = true;
                netClient->onWelcome([&](const fps::net::WelcomePacket& wp) {
                    myClientId = wp.clientId;
                    myTeam = wp.team;
                    std::printf("[fps_game] welcomed id=%u team=%u\n",
                                wp.clientId, wp.team);
                });
                netClient->onSnapshot([&](const fps::net::SnapshotPacket& sp) {
                    remoteEntities.clear();
                    lastScoreBlue = sp.scoreBlue;
                    lastScoreRed  = sp.scoreRed;
                    for (int i = 0; i < sp.entityCount; ++i) {
                        const auto& e = sp.entities[i];
                        if (e.kind == 0 && e.id == sp.yourId) {
                            lastAuthorityPos = Vector3{e.x, e.y, e.z};
                            player.health = e.health;
                            player.alive  = e.alive != 0;
                            hud.playerKills = e.kills;
                            continue;
                        }
                        RemoteEntity r{};
                        r.id       = e.id;
                        r.kind     = e.kind;
                        r.team     = e.team;
                        r.alive    = e.alive != 0;
                        r.health   = e.health;
                        r.kills    = e.kills;
                        r.position = Vector3{e.x, e.y, e.z};
                        r.yaw      = e.yaw;
                        r.pitch    = e.pitch;
                        remoteEntities.push_back(r);
                    }
                });
                netClient->onHitFeedback([&](const fps::net::HitFeedbackPacket& h) {
                    hud.hitMarkerTimer = 0.18f;
                    PlaySound(sfx.hitmarker);
                    if (h.killed) ++hud.playerKills;
                });
            } else {
                std::fprintf(stderr, "[fps_game] failed to connect to %s:%d; "
                                     "falling back to local sim.\n",
                             cfg.serverIp.c_str(), cfg.serverPort);
                netClient.reset();
            }
        }
    }
    // Networked: server owns bots, so clear the locally-spawned list.
    if (netConnected) bots.clear();

    float respawnTimer = 0.0f;
    float matchClock = 0.0f;

    const float FIXED_DT = 1.0f / 120.0f;
    float accumulator = 0.0f;
    float footstepTimer = 0.0f;
    bool  paused = false;
    bool  requestExit = false;

    // ----- In-game screen state ------------------------------------------
    // Default entry point is the main menu. Jump straight into a match for
    // team mode (the launcher already picked the map) or when the user
    // explicitly passes --skip-menu=1 from the command line.
    enum class GameScreen { MainMenu, InMatch };
    GameScreen screen = (wantNet || cfg.mode == "team") ? GameScreen::InMatch
                                                        : GameScreen::MainMenu;
    if (screen == GameScreen::MainMenu) EnableCursor();

    // Cycle the map/difficulty selection and rebuild the world accordingly.
    auto cycleMap = [&]() {
        if      (currentMapName == "range")     currentMapName = "warehouse";
        else if (currentMapName == "warehouse") currentMapName = "arena";
        else                                    currentMapName = "range";
    };
    auto cycleDifficulty = [&]() {
        if      (currentDifficulty == "easy")   currentDifficulty = "medium";
        else if (currentDifficulty == "medium") currentDifficulty = "hard";
        else                                    currentDifficulty = "easy";
        selectedDifficulty = parseDifficulty(currentDifficulty);
    };

    // Reload the world + respawn the player and bots. Called when the user
    // presses Play in the main menu (or after changing map/difficulty).
    auto startMatch = [&]() {
        world = Map::buildByName(currentMapName);
        selectedDifficulty = parseDifficulty(currentDifficulty);
        player.spawn(world.playerSpawn);
        for (Weapon& w : loadout) {
            w.ammoInMag = w.stats.magSize;
            w.reloadLeft = 0;
        }
        smokeUtility.ammoInMag = smokeUtility.stats.magSize;
        spawnBots();
        smokeProjs.clear();
        smokeVols.clear();
        tracers.clear();
        hud.playerKills = 0;
        matchClock = 0.0f;
        accumulator = 0.0f;
        paused = false;
        screen = GameScreen::InMatch;
        DisableCursor();
    };

    while (!WindowShouldClose() && !requestExit) {
        const float frameDt = std::min(GetFrameTime(), 0.1f);
        accumulator += frameDt;
        hud.matchTime = matchClock;

        // ========================= MAIN MENU SCREEN =========================
        if (screen == GameScreen::MainMenu) {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();
            const float cx = sw * 0.5f;

            const Rectangle playBtn{ cx - 170.0f, sh * 0.42f,        340.0f, 64.0f };
            const Rectangle mapBtn{  cx - 170.0f, sh * 0.42f +  84.0f, 340.0f, 48.0f };
            const Rectangle diffBtn{ cx - 170.0f, sh * 0.42f + 144.0f, 340.0f, 48.0f };
            const Rectangle quitBtn{ cx - 170.0f, sh * 0.42f + 220.0f, 340.0f, 44.0f };

            Vector2 mp = GetMousePosition();
            bool playH = CheckCollisionPointRec(mp, playBtn);
            bool mapH  = CheckCollisionPointRec(mp, mapBtn);
            bool diffH = CheckCollisionPointRec(mp, diffBtn);
            bool quitH = CheckCollisionPointRec(mp, quitBtn);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (playH)      startMatch();
                else if (mapH)  cycleMap();
                else if (diffH) cycleDifficulty();
                else if (quitH) requestExit = true;
            }
            // Keyboard shortcuts.
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) startMatch();
            if (IsKeyPressed(KEY_M)) cycleMap();
            if (IsKeyPressed(KEY_T)) cycleDifficulty();
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) requestExit = true;

            BeginDrawing();
            ClearBackground(Color{10, 12, 20, 255});
            // Radial vignette behind the logo.
            for (int i = 0; i < 12; ++i) {
                DrawCircle((int)cx, (int)(sh * 0.24f), 200.0f - i * 10.0f,
                           Color{static_cast<unsigned char>(30 + i * 3),
                                 20, 40, 10});
            }
            // Title.
            const char* title = "FPS //";
            int tw = MeasureText(title, 96);
            DrawText(title, (int)(cx - tw / 2), (int)(sh * 0.15f), 96,
                     Color{255, 70, 85, 255});
            const char* sub = "Valorant-inspired tactical FPS";
            int sw2 = MeasureText(sub, 20);
            DrawText(sub, (int)(cx - sw2 / 2), (int)(sh * 0.15f + 108), 20,
                     Color{140, 150, 170, 255});

            auto drawMenuBtn = [&](Rectangle r, const char* label, bool hover,
                                   Color accent, int fontSize) {
                DrawRectangleRec(r, hover ? accent : Color{26, 30, 44, 255});
                DrawRectangleLinesEx(r, hover ? 3 : 2, accent);
                int lw = MeasureText(label, fontSize);
                DrawText(label, (int)(r.x + r.width / 2 - lw / 2),
                         (int)(r.y + r.height / 2 - fontSize / 2), fontSize,
                         Color{240, 240, 250, 255});
            };
            drawMenuBtn(playBtn, "PLAY  \u25B6", playH,
                        Color{255, 70, 85, 255}, 34);

            char mapLabel[96], diffLabel[96];
            std::snprintf(mapLabel, sizeof(mapLabel),  "Map:  %s", currentMapName.c_str());
            std::snprintf(diffLabel, sizeof(diffLabel), "Difficulty:  %s",
                          currentDifficulty.c_str());
            drawMenuBtn(mapBtn,  mapLabel,  mapH,  Color{80, 150, 230, 255}, 22);
            drawMenuBtn(diffBtn, diffLabel, diffH, Color{80, 150, 230, 255}, 22);
            drawMenuBtn(quitBtn, "Quit to Desktop", quitH,
                        Color{180, 70, 80, 255}, 18);

            // Hints bottom.
            char nameLine[128];
            std::snprintf(nameLine, sizeof(nameLine), "Playing as %s", cfg.playerName.c_str());
            int nlw = MeasureText(nameLine, 16);
            DrawText(nameLine, (int)(cx - nlw / 2), sh - 60, 16,
                     Color{160, 170, 190, 255});
            const char* hint = "ENTER play   M map   T difficulty   Q quit";
            int hw = MeasureText(hint, 14);
            DrawText(hint, (int)(cx - hw / 2), sh - 32, 14,
                     Color{110, 120, 140, 230});

            EndDrawing();
            continue;
        }

        // ESC toggles the pause menu. When paused we release the cursor and
        // skip the fixed-timestep update so gameplay freezes.
        if (IsKeyPressed(KEY_ESCAPE)) {
            paused = !paused;
            if (paused) EnableCursor();
            else        DisableCursor();
            accumulator = 0.0f; // avoid a huge catch-up burst on resume
        }

        if (paused) {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();
            const Rectangle resumeBtn{sw / 2.0f - 140.0f, sh / 2.0f -  40.0f, 280.0f, 48.0f};
            const Rectangle menuBtn{  sw / 2.0f - 140.0f, sh / 2.0f +  28.0f, 280.0f, 48.0f};
            const Rectangle quitBtn{  sw / 2.0f - 140.0f, sh / 2.0f +  96.0f, 280.0f, 48.0f};
            Vector2 mp = GetMousePosition();
            bool resumeHover = CheckCollisionPointRec(mp, resumeBtn);
            bool menuHover   = CheckCollisionPointRec(mp, menuBtn);
            bool quitHover   = CheckCollisionPointRec(mp, quitBtn);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (resumeHover)      { paused = false; DisableCursor(); accumulator = 0.0f; }
                else if (menuHover)   { paused = false; screen = GameScreen::MainMenu; EnableCursor(); }
                else if (quitHover)   { requestExit = true; }
            }
            if (IsKeyPressed(KEY_Q)) requestExit = true;
        }

        while (!paused && accumulator >= FIXED_DT) {
            accumulator -= FIXED_DT;
            matchClock += FIXED_DT;

            bool firedThisTick = false;

            // Drain incoming server packets at the start of each tick.
            if (netClient) netClient->poll();

            if (hud.hitMarkerTimer > 0.0f)    hud.hitMarkerTimer    -= FIXED_DT;
            if (hud.damageFlashTimer > 0.0f)  hud.damageFlashTimer  -= FIXED_DT;

            Vector2 mouseDelta = GetMouseDelta();
            player.handleMouseLook(mouseDelta.x, mouseDelta.y);

            player.crouching = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_C);
            player.walking   = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

            Vector3 fwd = player.forward();
            fwd.y = 0;
            fwd = v3Norm(fwd);
            Vector3 right{std::cos(player.yaw), 0.0f, -std::sin(player.yaw)};
            Vector3 wish = v3Zero();
            if (IsKeyDown(KEY_W)) wish = v3Add(wish, fwd);
            if (IsKeyDown(KEY_S)) wish = v3Sub(wish, fwd);
            // D / A strafe: our 'right' basis vector points to screen-left under
            // Raylib's gluLookAt convention, so we subtract it for D and add for A
            // to match WASD expectations on screen.
            if (IsKeyDown(KEY_D)) wish = v3Sub(wish, right);
            if (IsKeyDown(KEY_A)) wish = v3Add(wish, right);
            float wishLen = v3Len(wish);
            if (wishLen > 0.0f) wish = v3Scale(wish, 1.0f / wishLen);
            float targetSpeed = player.speedMultiplier();

            float wishSpeed = wishLen > 0.0f ? targetSpeed : 0.0f;
            float accel = player.onGround ? 80.0f : 25.0f;
            float currentSpeed = v3Dot(player.velocity, wish);
            float addSpeed = wishSpeed - currentSpeed;
            if (addSpeed > 0.0f && wishLen > 0.0f) {
                float accelSpeed = std::min(accel * FIXED_DT * wishSpeed, addSpeed);
                player.velocity.x += wish.x * accelSpeed;
                player.velocity.z += wish.z * accelSpeed;
            }

            if (IsKeyPressed(KEY_SPACE) && player.onGround && player.alive) {
                player.velocity.y = 7.5f;
                player.onGround = false;
            }

            if (IsKeyPressed(KEY_ONE))   currentWeapon = 0;
            if (IsKeyPressed(KEY_TWO))   currentWeapon = 1;
            if (IsKeyPressed(KEY_THREE)) currentWeapon = 2;
            if (IsKeyPressed(KEY_FOUR))  currentWeapon = 3;
            if (IsKeyPressed(KEY_FIVE))  currentWeapon = 4;
            if (IsKeyPressed(KEY_SIX))   currentWeapon = 5;
            float wheel = GetMouseWheelMove();
            if (wheel > 0.1f)      currentWeapon = (currentWeapon + 1) % static_cast<int>(loadout.size());
            else if (wheel < -0.1f) currentWeapon = (currentWeapon - 1 + static_cast<int>(loadout.size())) % static_cast<int>(loadout.size());
            if (IsKeyPressed(KEY_R)) {
                if (loadout[currentWeapon].reloadLeft <= 0.0f &&
                    loadout[currentWeapon].ammoInMag != loadout[currentWeapon].stats.magSize) {
                    PlaySound(sfx.reload);
                }
                loadout[currentWeapon].startReload();
            }

            Weapon& w = loadout[currentWeapon];
            // Track reload progress to trigger a "bolt slam" click partway
            // through the animation for that crispy mid-reload feel.
            static float prevReloadProg = 0.0f;
            float curReloadProg = 0.0f;
            if (w.reloadLeft > 0.0f && w.stats.reloadTime > 0.0f) {
                curReloadProg = 1.0f - (w.reloadLeft / w.stats.reloadTime);
            }
            if (prevReloadProg < 0.82f && curReloadProg >= 0.82f) {
                PlaySound(sfx.reload);
            }
            prevReloadProg = curReloadProg;

            w.tick(FIXED_DT);
            smokeUtility.tick(FIXED_DT);
            if (muzzle.life > 0.0f) muzzle.life -= FIXED_DT;

            bool wantsFire = w.stats.automatic
                ? IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            if (player.alive && wantsFire && w.canFire()) {
                Vector3 eye = player.eyePosition();
                Vector3 aim = player.forward();

                if (w.stats.kind == WeaponKind::Knife) {
                    float dist;
                    int wallIdx;
                    float hitDist = w.stats.knifeRange;
                    raycastMap(world, eye, aim, hitDist, dist, wallIdx);
                    float best = std::min(dist > 0 ? dist : hitDist, hitDist);
                    int   hitBotIdx = -1;
                    bool  hs = false;
                    for (size_t i = 0; i < bots.size(); ++i) {
                        float t; bool h;
                        if (rayHitsBot(eye, aim, best, bots[i], t, h) && t < best) {
                            best = t; hitBotIdx = static_cast<int>(i); hs = h;
                        }
                    }
                    if (!netConnected && hitBotIdx >= 0) {
                        int dmg = hs ? w.stats.damageHead : w.stats.damageBody;
                        bots[hitBotIdx].applyDamage(dmg);
                        hud.hitMarkerTimer = 0.18f;
                        PlaySound(sfx.hitmarker);
                        if (!bots[hitBotIdx].alive) ++hud.playerKills;
                    }
                    // Knife in multiplayer: short-range authoritative attack.
                    if (netConnected && netClient) {
                        fps::net::FirePacket fp{};
                        fp.sequence = nextInputSeq;
                        fp.weaponSlot = static_cast<uint8_t>(currentWeapon);
                        fp.weaponKind = 2;
                        fp.originX = eye.x; fp.originY = eye.y; fp.originZ = eye.z;
                        fp.dirX = aim.x; fp.dirY = aim.y; fp.dirZ = aim.z;
                        fp.maxRange = w.stats.knifeRange;
                        fp.bodyDamage = static_cast<uint8_t>(std::min(w.stats.damageBody, 255));
                        fp.headshotDamage = static_cast<uint8_t>(std::min(w.stats.damageHead, 255));
                        netClient->sendFire(fp);
                    }
                    PlaySound(sfx.knife);
                    w.cooldown = w.stats.fireInterval;
                    w.sinceLastShot = 0.0f;
                    firedThisTick = true;
                } else {
                    Vector3 aimDir = aim;
                    Vector3 sprayOff{0, 0, 0};
                    if (w.stats.kind == WeaponKind::Rifle) {
                        sprayOff = vandalSprayOffset(w.shotsInBurst++);
                    }
                    float moveSpeed = v3Len(Vector3{player.velocity.x, 0, player.velocity.z});
                    float spreadMag = w.stats.baseSpread +
                                      (moveSpeed > 0.5f ? w.stats.movingSpread : 0.0f);
                    const float invRand = 1.0f / static_cast<float>(RAND_MAX);
                    float jx = (static_cast<float>(std::rand()) * invRand - 0.5f) * 2.0f * spreadMag;
                    float jy = (static_cast<float>(std::rand()) * invRand - 0.5f) * 2.0f * spreadMag;
                    float yawOff   = sprayOff.x + jx;
                    float pitchOff = sprayOff.y + jy;
                    Vector3 rightV{std::cos(player.yaw), 0.0f, -std::sin(player.yaw)};
                    Vector3 upV{0, 1, 0};
                    aimDir = v3Add(aimDir, v3Scale(rightV, yawOff));
                    aimDir = v3Add(aimDir, v3Scale(upV, pitchOff));
                    aimDir = v3Norm(aimDir);

                    float maxD = w.stats.maxRange;
                    float wallDist = maxD;
                    int wallIdx;
                    raycastMap(world, eye, aimDir, maxD, wallDist, wallIdx);

                    int hitBotIdx = -1;
                    bool hs = false;
                    float best = wallDist;
                    for (size_t i = 0; i < bots.size(); ++i) {
                        float t; bool h;
                        if (rayHitsBot(eye, aimDir, best, bots[i], t, h) && t < best) {
                            best = t; hitBotIdx = static_cast<int>(i); hs = h;
                        }
                    }
                    bool blocked = rayIntersectsSmoke(smokeVols, eye, aimDir, best);

                    Vector3 hitPoint = v3Add(eye, v3Scale(aimDir, best));
                    TracerEffect tr{};
                    tr.from = eye;
                    tr.to   = hitPoint;
                    tr.life = 0.09f;
                    tr.color = blocked ? Color{180, 200, 220, 180} : Color{255, 240, 200, 220};
                    tracers.push_back(tr);

                    // Local hit resolution only when running the solo sim.
                    if (!netConnected && !blocked && hitBotIdx >= 0) {
                        int dmg = hs ? w.stats.damageHead : w.stats.damageBody;
                        bots[hitBotIdx].applyDamage(dmg);
                        hud.hitMarkerTimer = 0.18f;
                        PlaySound(sfx.hitmarker);
                        if (!bots[hitBotIdx].alive) ++hud.playerKills;
                    }

                    // Networked: let the server resolve damage authoritatively.
                    if (netConnected && netClient) {
                        fps::net::FirePacket fp{};
                        fp.sequence = nextInputSeq;
                        fp.weaponSlot = static_cast<uint8_t>(currentWeapon);
                        fp.weaponKind = 0; // rifle/pistol - informational
                        fp.originX = eye.x; fp.originY = eye.y; fp.originZ = eye.z;
                        fp.dirX = aimDir.x; fp.dirY = aimDir.y; fp.dirZ = aimDir.z;
                        fp.maxRange = w.stats.maxRange;
                        fp.bodyDamage = static_cast<uint8_t>(std::min(w.stats.damageBody, 255));
                        fp.headshotDamage = static_cast<uint8_t>(std::min(w.stats.damageHead, 255));
                        netClient->sendFire(fp);
                    }

                    PlaySound(w.stats.kind == WeaponKind::Pistol ? sfx.pistol : sfx.rifle);
                    w.ammoInMag--;
                    w.cooldown = w.stats.fireInterval;
                    w.sinceLastShot = 0.0f;
                    muzzle.life = 0.06f;
                    firedThisTick = true;
                }
            }

            if (player.alive && IsKeyPressed(KEY_G) && smokeUtility.canFire()) {
                smokeProjs.push_back(throwSmoke(player.eyePosition(), player.forward()));
                smokeUtility.ammoInMag--;
                smokeUtility.cooldown = smokeUtility.stats.fireInterval;
                PlaySound(sfx.smokeThrow);
                firedThisTick = true;
            }

            footstepTimer += FIXED_DT;
            float horiz = v3Len(Vector3{player.velocity.x, 0, player.velocity.z});
            float footInterval = player.crouching ? 0.6f : (player.walking ? 0.5f : 0.35f);
            if (player.onGround && horiz > 1.2f && footstepTimer >= footInterval) {
                footstepTimer = 0.0f;
                if (!player.walking) PlaySound(sfx.footstep);
            }

            player.update(world, FIXED_DT);
            size_t smokeCountBefore = smokeVols.size();
            tickSmokes(smokeProjs, smokeVols, world, FIXED_DT);
            if (smokeVols.size() > smokeCountBefore) PlaySound(sfx.smokePuff);

            // Local bot AI (solo mode only). In team mode the server owns
            // the world state; remote players + server-driven bots are
            // rendered from snapshots.
            if (!netConnected) {
                std::vector<BotShot> shots;
                for (Bot& b : bots) {
                    updateBot(b, player, world, smokeVols, FIXED_DT, shots);
                }
                for (const BotShot& s : shots) {
                    player.applyDamage(s.damage);
                    hud.damageFlashTimer = 0.5f;
                    (void)s.botId;
                }

                int alive = 0;
                for (const Bot& b : bots) if (b.alive) ++alive;
                hud.botsAlive = alive;

                if (alive == 0 && !bots.empty()) {
                    for (size_t i = 0; i < bots.size(); ++i) {
                        bots[i].alive = true;
                        bots[i].health = 100;
                        bots[i].armor = 25;
                        bots[i].state = BotState::Patrol;
                        bots[i].path.clear();
                        bots[i].pathIdx = 0;
                        bots[i].position = world.botSpawns[i % world.botSpawns.size()];
                    }
                }

                if (!player.alive) {
                    respawnTimer -= FIXED_DT;
                    if (respawnTimer <= 0.0f) {
                        player.spawn(world.playerSpawn);
                        for (Weapon& lw : loadout) { lw.ammoInMag = lw.stats.magSize; lw.reloadLeft = 0; }
                        smokeUtility.ammoInMag = smokeUtility.stats.magSize;
                    }
                } else {
                    respawnTimer = 5.0f;
                }
            } else {
                // Networked: server drives life/death. We still show the death
                // overlay + countdown locally.
                int aliveCount = 0;
                for (const RemoteEntity& re : remoteEntities)
                    if (re.alive) ++aliveCount;
                hud.botsAlive = aliveCount;
                if (!player.alive) {
                    respawnTimer -= FIXED_DT;
                    if (respawnTimer < 0.0f) respawnTimer = 0.0f;
                } else {
                    respawnTimer = 5.0f;
                }
                // Soft reconciliation: if the server says we're significantly
                // elsewhere, snap toward it. Otherwise trust local prediction.
                float divergence = v3Len(v3Sub(player.position, lastAuthorityPos));
                if (divergence > 3.0f) {
                    player.position = lastAuthorityPos;
                    player.velocity = v3Zero();
                } else if (divergence > 0.5f) {
                    // Gentle blend for small drift.
                    player.position = v3Add(
                        v3Scale(player.position, 0.85f),
                        v3Scale(lastAuthorityPos, 0.15f));
                }

                // Send input to the server.
                if (netClient && netClient->connected()) {
                    PlayerInput in{};
                    in.sequence = ++nextInputSeq;
                    in.dt = FIXED_DT;
                    in.yaw = player.yaw;
                    in.pitch = player.pitch;
                    in.moveForward = IsKeyDown(KEY_W) ? 1 : (IsKeyDown(KEY_S) ? -1 : 0);
                    in.moveRight   = IsKeyDown(KEY_D) ? 1 : (IsKeyDown(KEY_A) ? -1 : 0);
                    in.buttons = 0;
                    if (IsKeyDown(KEY_SPACE))            in.buttons |= 0x01;
                    if (player.crouching)                in.buttons |= 0x02;
                    if (player.walking)                  in.buttons |= 0x04;
                    in.weaponSlot = static_cast<uint8_t>(currentWeapon);
                    netClient->sendInput(in);
                }
            }

            for (TracerEffect& t : tracers) t.life -= FIXED_DT;
            tracers.erase(
                std::remove_if(tracers.begin(), tracers.end(),
                               [](const TracerEffect& t) { return t.life <= 0.0f; }),
                tracers.end());

            // Viewmodel state: bob + kick + swap animation.
            float horizSpeedVm = v3Len(Vector3{player.velocity.x, 0, player.velocity.z});
            tickViewModel(vm, FIXED_DT, horizSpeedVm, firedThisTick, currentWeapon);
        }

        Camera3D cam = player.buildCamera();
        BeginDrawing();
        ClearBackground(Color{12, 14, 22, 255});
        BeginMode3D(cam);

        for (const Wall& wl : world.walls) {
            Vector3 center = v3Scale(v3Add(wl.box.min, wl.box.max), 0.5f);
            Vector3 size   = v3Sub(wl.box.max, wl.box.min);
            DrawCubeV(center, size, wl.color);
            if (!wl.isFloor) {
                DrawCubeWiresV(center, size, Color{20, 22, 32, 255});
            }
        }

        if (IsKeyDown(KEY_F1)) {
            for (const NavWaypoint& w : world.navGraph) {
                DrawSphere(w.position, 0.12f, Color{80, 160, 240, 180});
                for (int n : w.neighbors) {
                    DrawLine3D(w.position, world.navGraph[n].position,
                               Color{40, 100, 160, 120});
                }
            }
        }

        for (const Bot& b : bots) drawBot(b);

        // In networked mode, render remote players + server-owned bots.
        for (const RemoteEntity& re : remoteEntities) drawRemoteAgent(re);

        for (const SmokeProjectile& p : smokeProjs) {
            DrawSphere(p.position, 0.12f, Color{180, 180, 180, 255});
        }
        for (const SmokeVolume& v : smokeVols) {
            float fadeIn  = clampf(v.age * 2.0f, 0.0f, 1.0f);
            float fadeOut = clampf((v.lifetime - v.age) * 0.8f, 0.0f, 1.0f);
            float a = std::min(fadeIn, fadeOut);
            const int layers = 6;
            for (int i = 0; i < layers; ++i) {
                float r = v.radius * (0.55f + 0.08f * i);
                unsigned char alpha = static_cast<unsigned char>(50 * a);
                DrawSphere(v.center, r, Color{210, 215, 220, alpha});
            }
        }

        for (const TracerEffect& t : tracers) {
            DrawLine3D(t.from, t.to, t.color);
        }

        EndMode3D();

        if (muzzle.life > 0.0f) {
            int cx = GetScreenWidth() / 2;
            int cy = GetScreenHeight() / 2;
            unsigned char a = static_cast<unsigned char>(muzzle.life * 2000.0f);
            DrawCircle(cx, cy + 14, 16.0f + muzzle.life * 100.0f,
                       Color{255, 220, 120, a > 160 ? (unsigned char)160 : a});
        }

        // 3D viewmodel (right-hand gun + arms). Drawn after the world scene
        // but before the HUD so the crosshair, ammo, etc. sit on top.
        drawViewModel(vm, loadout[currentWeapon]);

        drawHud(hud, player, loadout[currentWeapon],
                GetScreenWidth(), GetScreenHeight(), cfg.playerName.c_str());

        // Team scoreboard ribbon (networked mode only).
        if (netConnected) {
            int sw = GetScreenWidth();
            int cx = sw / 2;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%u   :   %u",
                          static_cast<unsigned>(lastScoreBlue),
                          static_cast<unsigned>(lastScoreRed));
            int tw = MeasureText(buf, 36);
            DrawRectangle(cx - tw / 2 - 24, 58, tw + 48, 44, Color{0, 0, 0, 160});
            DrawText("BLUE", cx - tw / 2 - 70, 68, 18, Color{80, 150, 230, 255});
            DrawText("RED",  cx + tw / 2 + 20, 68, 18, Color{230, 80, 90, 255});
            DrawText(buf, cx - tw / 2, 62, 36,
                     myTeam == 0 ? Color{150, 210, 255, 255}
                                 : Color{255, 150, 160, 255});
            // "Connected" indicator.
            const char* tag = myTeam == 0 ? "TEAM BLUE" : "TEAM RED";
            int tagW = MeasureText(tag, 16);
            DrawRectangle(sw - tagW - 28, 16, tagW + 20, 28, Color{0, 0, 0, 160});
            DrawText(tag, sw - tagW - 18, 22, 16, teamColor(myTeam));
        } else if (wantNet) {
            // Failed to connect - warn the user.
            const char* msg = "Server unreachable - running local sim";
            int tw = MeasureText(msg, 16);
            DrawRectangle(GetScreenWidth() / 2 - tw / 2 - 12, 60, tw + 24, 26,
                          Color{120, 30, 40, 220});
            DrawText(msg, GetScreenWidth() / 2 - tw / 2, 64, 16,
                     Color{255, 220, 220, 255});
        }

        if (!player.alive && !paused) {
            drawDeathOverlay(GetScreenWidth(), GetScreenHeight(), respawnTimer);
        }

        if (paused) {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();
            DrawRectangle(0, 0, sw, sh, Color{8, 10, 16, 200});
            const char* title = "PAUSED";
            int tw = MeasureText(title, 56);
            DrawText(title, sw / 2 - tw / 2, sh / 2 - 150, 56, Color{230, 235, 255, 255});

            // Map + difficulty context line.
            char ctx[128];
            const char* diffStr = cfg.difficulty == "easy"   ? "Easy" :
                                  cfg.difficulty == "hard"   ? "Hard" : "Medium";
            std::snprintf(ctx, sizeof(ctx), "Map: %s   Difficulty: %s",
                          world.displayName.c_str(), diffStr);
            int cw = MeasureText(ctx, 18);
            DrawText(ctx, sw / 2 - cw / 2, sh / 2 - 80, 18, Color{160, 170, 200, 255});

            Vector2 mp = GetMousePosition();
            const Rectangle resumeBtn{sw / 2.0f - 140.0f, sh / 2.0f -  40.0f, 280.0f, 48.0f};
            const Rectangle menuBtn{  sw / 2.0f - 140.0f, sh / 2.0f +  28.0f, 280.0f, 48.0f};
            const Rectangle quitBtn{  sw / 2.0f - 140.0f, sh / 2.0f +  96.0f, 280.0f, 48.0f};
            bool resumeHover = CheckCollisionPointRec(mp, resumeBtn);
            bool menuHover   = CheckCollisionPointRec(mp, menuBtn);
            bool quitHover   = CheckCollisionPointRec(mp, quitBtn);

            auto drawBtn = [](Rectangle r, const char* label, bool hover, Color accent) {
                DrawRectangleRec(r, hover ? accent : Color{30, 34, 48, 255});
                DrawRectangleLinesEx(r, 2, accent);
                int lw = MeasureText(label, 20);
                DrawText(label, (int)(r.x + r.width / 2 - lw / 2),
                         (int)(r.y + r.height / 2 - 10), 20, Color{240, 240, 250, 255});
            };
            drawBtn(resumeBtn, "Resume (Esc)",         resumeHover, Color{80, 200, 130, 255});
            drawBtn(menuBtn,   "Main Menu",            menuHover,   Color{80, 150, 230, 255});
            drawBtn(quitBtn,   "Quit to Desktop (Q)",  quitHover,   Color{200, 80, 90, 255});
        }

        EndDrawing();
    }

    if (netClient) {
        netClient->disconnect();
        netClient.reset();
        fps::net::NetClient::globalShutdown();
    }
    sfx.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

} // namespace fps
