#include "App.h"

#include "../ai/Bot.h"
#include "../player/Player.h"
#include "../render/Hud.h"
#include "../render/SoundBank.h"
#include "../weapons/Smoke.h"
#include "../weapons/Weapon.h"
#include "../world/Map.h"
#include "Math.h"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

    Map world = Map::buildDefault();

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
    const BotDifficulty difficulties[] = {
        BotDifficulty::Easy, BotDifficulty::Normal, BotDifficulty::Hard,
    };
    int bId = 0;
    for (size_t i = 0; i < world.botSpawns.size(); ++i) {
        Bot b{};
        b.id = bId++;
        b.position = world.botSpawns[i];
        b.difficulty = difficulties[i % 3];
        b.weapon = Weapon::makeClassic();
        bots.push_back(b);
    }

    std::vector<SmokeProjectile> smokeProjs;
    std::vector<SmokeVolume>     smokeVols;
    std::vector<TracerEffect>    tracers;
    MuzzleFlashEffect muzzle{};

    HudState hud{};
    hud.botsAlive = static_cast<int>(bots.size());

    float respawnTimer = 0.0f;
    float matchClock = 0.0f;

    const float FIXED_DT = 1.0f / 120.0f;
    float accumulator = 0.0f;
    float footstepTimer = 0.0f;

    while (!WindowShouldClose()) {
        const float frameDt = std::min(GetFrameTime(), 0.1f);
        accumulator += frameDt;
        hud.matchTime = matchClock;

        while (accumulator >= FIXED_DT) {
            accumulator -= FIXED_DT;
            matchClock += FIXED_DT;

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
            if (IsKeyDown(KEY_D)) wish = v3Add(wish, right);
            if (IsKeyDown(KEY_A)) wish = v3Sub(wish, right);
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
                    if (hitBotIdx >= 0) {
                        int dmg = hs ? w.stats.damageHead : w.stats.damageBody;
                        bots[hitBotIdx].applyDamage(dmg);
                        hud.hitMarkerTimer = 0.18f;
                        PlaySound(sfx.hitmarker);
                        if (!bots[hitBotIdx].alive) ++hud.playerKills;
                    }
                    PlaySound(sfx.knife);
                    w.cooldown = w.stats.fireInterval;
                    w.sinceLastShot = 0.0f;
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

                    if (!blocked && hitBotIdx >= 0) {
                        int dmg = hs ? w.stats.damageHead : w.stats.damageBody;
                        bots[hitBotIdx].applyDamage(dmg);
                        hud.hitMarkerTimer = 0.18f;
                        PlaySound(sfx.hitmarker);
                        if (!bots[hitBotIdx].alive) ++hud.playerKills;
                    }

                    PlaySound(w.stats.kind == WeaponKind::Pistol ? sfx.pistol : sfx.rifle);
                    w.ammoInMag--;
                    w.cooldown = w.stats.fireInterval;
                    w.sinceLastShot = 0.0f;
                    muzzle.life = 0.06f;
                }
            }

            if (player.alive && IsKeyPressed(KEY_G) && smokeUtility.canFire()) {
                smokeProjs.push_back(throwSmoke(player.eyePosition(), player.forward()));
                smokeUtility.ammoInMag--;
                smokeUtility.cooldown = smokeUtility.stats.fireInterval;
                PlaySound(sfx.smokeThrow);
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

            if (alive == 0) {
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

            for (TracerEffect& t : tracers) t.life -= FIXED_DT;
            tracers.erase(
                std::remove_if(tracers.begin(), tracers.end(),
                               [](const TracerEffect& t) { return t.life <= 0.0f; }),
                tracers.end());
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

        {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            int wx = sw - 280;
            int wy = sh - 220;
            const Weapon& cw = loadout[currentWeapon];
            Color vm{40, 45, 60, 230};
            Color accent{80, 90, 120, 255};
            if (cw.stats.kind == WeaponKind::Knife) {
                DrawRectangle(wx + 40, wy + 10, 120, 12, vm);
                DrawRectangle(wx + 30, wy + 22, 10, 22, accent);
            } else {
                DrawRectangle(wx, wy + 60, 220, 30, vm);
                DrawRectangle(wx + 60, wy + 90, 40, 60, vm);
                DrawRectangle(wx + 160, wy + 40, 28, 20, accent);
                if (cw.stats.kind == WeaponKind::Rifle) {
                    DrawRectangle(wx - 30, wy + 66, 60, 14, vm);
                }
            }
        }

        drawHud(hud, player, loadout[currentWeapon],
                GetScreenWidth(), GetScreenHeight(), cfg.playerName.c_str());

        if (!player.alive) {
            drawDeathOverlay(GetScreenWidth(), GetScreenHeight(), respawnTimer);
        }

        EndDrawing();
    }

    sfx.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

} // namespace fps
