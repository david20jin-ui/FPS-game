// Headless authoritative game server for team deathmatch.
//
// Responsibilities:
//   - Accept enet connections and assign each to a team (alternating).
//   - Run an authoritative 64 Hz simulation of every player's position.
//   - Resolve every fire event by raycasting against all players (and bots
//     in solo mode), apply damage, handle kills + respawn.
//   - Broadcast snapshots that include team assignment, health, kills, and
//     smoke volumes.
#include "ai/Bot.h"
#include "core/Cli.h"
#include "net/NetServer.h"
#include "player/Player.h"
#include "weapons/Smoke.h"
#include "weapons/Weapon.h"
#include "world/Map.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace fps;
using namespace fps::net;

namespace {

constexpr float RESPAWN_SECONDS = 5.0f;

struct ServerSession {
    uint8_t  clientId;
    uint8_t  team = 0;
    Player   player;
    int      kills = 0;
    float    respawnTimer = 0.0f;
};

// Hitscan: ray vs vertical cylinder approximating a player.
bool rayHitsCylinder(Vector3 origin, Vector3 dir, float maxDist,
                     Vector3 basePos, float radius, float height,
                     float& outT, bool& outHeadshot) {
    Vector3 oc{origin.x - basePos.x, 0.0f, origin.z - basePos.z};
    Vector3 d{dir.x, 0.0f, dir.z};
    float A = d.x * d.x + d.z * d.z;
    float B = 2.0f * (oc.x * d.x + oc.z * d.z);
    float C = oc.x * oc.x + oc.z * oc.z - radius * radius;
    if (A < 1e-6f) return false;
    float disc = B * B - 4 * A * C;
    if (disc < 0) return false;
    float sq = std::sqrt(disc);
    float t1 = (-B - sq) / (2 * A);
    float t2 = (-B + sq) / (2 * A);
    float t = t1 >= 0 ? t1 : t2;
    if (t < 0 || t > maxDist) return false;
    float hitY = origin.y + dir.y * t;
    if (hitY < basePos.y || hitY > basePos.y + height) return false;
    outT = t;
    outHeadshot = hitY >= basePos.y + height * 0.88f;
    return true;
}

// Team-based spawn point: pick the nearest configured spawn to the team's
// preferred area so teams start at opposite ends of the map.
Vector3 spawnForTeam(const Map& world, uint8_t team) {
    if (team == 0) return world.playerSpawn;
    // Team 1 spawns on the far side (where bots usually start).
    if (!world.botSpawns.empty()) return world.botSpawns[0];
    return Vector3{0.0f, 1.0f, 10.0f};
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    Config cfg = parseCli(argc, argv);
    uint16_t port = static_cast<uint16_t>(cfg.serverPort == 0 ? 7777 : cfg.serverPort);
    const bool teamMode = (cfg.mode == "team");

    if (!NetServer::globalInit()) {
        std::fprintf(stderr, "[fps_server] enet init failed\n");
        return 1;
    }

    NetServer server;
    if (!server.listen(port)) {
        std::fprintf(stderr, "[fps_server] failed to bind port %u\n", port);
        NetServer::globalShutdown();
        return 1;
    }
    std::printf("[fps_server] listening on :%u mode=%s map=%s\n",
                port, cfg.mode.c_str(), cfg.mapName.c_str());

    Map world = Map::buildByName(cfg.mapName);

    // Bots only for solo mode.
    std::vector<Bot> bots;
    if (!teamMode) {
        auto parseDifficulty = [](const std::string& s) {
            if (s == "easy")   return BotDifficulty::Easy;
            if (s == "hard")   return BotDifficulty::Hard;
            return BotDifficulty::Normal;
        };
        BotDifficulty diff = parseDifficulty(cfg.difficulty);
        int id = 0;
        for (size_t i = 0; i < world.botSpawns.size(); ++i) {
            Bot b{};
            b.id = id++;
            b.position = world.botSpawns[i];
            b.difficulty = diff;
            bots.push_back(b);
        }
    }

    std::vector<SmokeProjectile> smokeProjs;
    std::vector<SmokeVolume>     smokeVols;

    std::unordered_map<uint8_t, ServerSession> sessions;

    // Join handler: alternating team assignment (0, 1, 0, 1, ...).
    server.onJoin([&](ConnectedClient& c) {
        // Count existing clients to decide team.
        int t0 = 0, t1 = 0;
        for (const auto& kv : sessions) {
            if (kv.second.team == 0) ++t0;
            else                     ++t1;
        }
        c.team = (t0 <= t1) ? 0 : 1;

        ServerSession s{};
        s.clientId = c.clientId;
        s.team = c.team;
        s.player.spawn(spawnForTeam(world, c.team));
        sessions[c.clientId] = s;
        std::printf("[fps_server] client %u joined team %u\n", c.clientId, c.team);
    });

    uint8_t scoreBlue = 0, scoreRed = 0;

    constexpr int    TICK_RATE = 64;
    constexpr double DT = 1.0 / TICK_RATE;
    uint32_t tick = 0;
    auto nextTick = std::chrono::steady_clock::now();

    while (true) {
        server.poll();

        // ----- Apply queued inputs per client ----------------------------
        for (ConnectedClient* c : server.clients()) {
            auto it = sessions.find(c->clientId);
            if (it == sessions.end()) continue;
            ServerSession& s = it->second;

            while (!c->pending.empty()) {
                PlayerInput in = c->pending.front();
                c->pending.pop_front();

                s.player.yaw   = in.yaw;
                s.player.pitch = in.pitch;
                s.player.crouching = (in.buttons & 0x02) != 0;
                s.player.walking   = (in.buttons & 0x04) != 0;

                Vector3 fwd = s.player.forward(); fwd.y = 0; fwd = v3Norm(fwd);
                // Use the same right-hand convention as the client (see App.cpp).
                Vector3 right{std::cos(s.player.yaw), 0.0f, -std::sin(s.player.yaw)};
                Vector3 wish = v3Zero();
                wish = v3Add(wish, v3Scale(fwd,   static_cast<float>(in.moveForward)));
                wish = v3Sub(wish, v3Scale(right, static_cast<float>(in.moveRight)));
                float wl = v3Len(wish);
                if (wl > 0) wish = v3Scale(wish, 1.0f / wl);
                float targetSpeed = s.player.speedMultiplier();
                float wishSpeed = wl > 0 ? targetSpeed : 0.0f;
                float accel = s.player.onGround ? 80.0f : 25.0f;
                float cur = v3Dot(s.player.velocity, wish);
                float add = wishSpeed - cur;
                if (add > 0 && wl > 0) {
                    float as = std::min(accel * in.dt * wishSpeed, add);
                    s.player.velocity.x += wish.x * as;
                    s.player.velocity.z += wish.z * as;
                }
                if ((in.buttons & 0x01) && s.player.onGround && s.player.alive) {
                    s.player.velocity.y = 7.5f;
                    s.player.onGround = false;
                }
                s.player.update(world, in.dt);
                c->lastAck = in.sequence;
            }
        }

        // ----- Resolve fire events authoritatively -----------------------
        for (ConnectedClient* c : server.clients()) {
            auto it = sessions.find(c->clientId);
            if (it == sessions.end()) continue;
            ServerSession& attacker = it->second;
            if (!attacker.player.alive) { c->pendingFires.clear(); continue; }

            while (!c->pendingFires.empty()) {
                FirePacket fp = c->pendingFires.front();
                c->pendingFires.pop_front();

                Vector3 origin{fp.originX, fp.originY, fp.originZ};
                Vector3 dir{fp.dirX, fp.dirY, fp.dirZ};
                float len = v3Len(dir);
                if (len < 1e-4f) continue;
                dir = v3Scale(dir, 1.0f / len);

                float wallDist = fp.maxRange;
                int   wallIdx;
                raycastMap(world, origin, dir, fp.maxRange, wallDist, wallIdx);

                // Find the closest hittable target along the ray.
                ServerSession* hitTarget = nullptr;
                bool hitIsHeadshot = false;
                float bestT = wallDist;

                for (auto& kv : sessions) {
                    ServerSession& tgt = kv.second;
                    if (tgt.clientId == attacker.clientId) continue;
                    if (tgt.team == attacker.team) continue; // no team-kill
                    if (!tgt.player.alive) continue;
                    float t; bool hs;
                    if (rayHitsCylinder(origin, dir, bestT, tgt.player.position,
                                        Player::RADIUS,
                                        tgt.player.currentHeight(), t, hs)) {
                        if (t < bestT) { bestT = t; hitTarget = &tgt; hitIsHeadshot = hs; }
                    }
                }

                // Smokes block shots.
                bool blocked = rayIntersectsSmoke(smokeVols, origin, dir, bestT);
                if (blocked) continue;

                if (hitTarget) {
                    int damage = hitIsHeadshot ? fp.headshotDamage : fp.bodyDamage;
                    bool wasAlive = hitTarget->player.alive;
                    hitTarget->player.applyDamage(damage);
                    bool killed = wasAlive && !hitTarget->player.alive;
                    if (killed) {
                        attacker.kills++;
                        if (attacker.team == 0) ++scoreBlue; else ++scoreRed;
                        hitTarget->respawnTimer = RESPAWN_SECONDS;
                    }

                    HitFeedbackPacket hp{};
                    hp.targetId = hitTarget->clientId;
                    hp.damage = static_cast<uint8_t>(damage);
                    hp.headshot = hitIsHeadshot ? 1 : 0;
                    hp.killed = killed ? 1 : 0;
                    server.sendHitFeedback(*c, hp);
                } else {
                    // Check bots too (solo mode).
                    int bi = -1; bool bhs = false;
                    for (size_t i = 0; i < bots.size(); ++i) {
                        if (!bots[i].alive) continue;
                        float t; bool hs;
                        if (rayHitsCylinder(origin, dir, bestT, bots[i].position,
                                            bots[i].radius(), bots[i].height(), t, hs)) {
                            if (t < bestT) { bestT = t; bi = static_cast<int>(i); bhs = hs; }
                        }
                    }
                    if (bi >= 0) {
                        int damage = bhs ? fp.headshotDamage : fp.bodyDamage;
                        bool wasAlive = bots[bi].alive;
                        bots[bi].applyDamage(damage);
                        bool killed = wasAlive && !bots[bi].alive;
                        if (killed) attacker.kills++;
                        HitFeedbackPacket hp{};
                        hp.targetId = static_cast<uint8_t>(100 + bi);
                        hp.damage = static_cast<uint8_t>(damage);
                        hp.headshot = bhs ? 1 : 0;
                        hp.killed = killed ? 1 : 0;
                        server.sendHitFeedback(*c, hp);
                    }
                }
            }
        }

        // ----- Bot AI (solo mode only) -----------------------------------
        Player* aiTarget = nullptr;
        for (auto& kv : sessions) {
            if (kv.second.player.alive) { aiTarget = &kv.second.player; break; }
        }
        std::vector<BotShot> botShots;
        if (!teamMode && aiTarget) {
            for (Bot& b : bots)
                updateBot(b, *aiTarget, world, smokeVols,
                          static_cast<float>(DT), botShots);
            for (const BotShot& bs : botShots)
                aiTarget->applyDamage(bs.damage);
        }
        tickSmokes(smokeProjs, smokeVols, world, static_cast<float>(DT));

        // ----- Respawn dead players --------------------------------------
        for (auto& kv : sessions) {
            ServerSession& s = kv.second;
            if (s.player.alive) continue;
            s.respawnTimer -= static_cast<float>(DT);
            if (s.respawnTimer <= 0.0f) {
                s.player.spawn(spawnForTeam(world, s.team));
            }
        }

        // ----- Build and send per-client snapshots -----------------------
        SnapshotPacket base{};
        base.tick = tick++;
        base.scoreBlue = scoreBlue;
        base.scoreRed  = scoreRed;

        uint8_t ec = 0;
        for (auto& kv : sessions) {
            if (ec >= MAX_PLAYERS) break;
            const ServerSession& s = kv.second;
            EntitySnapshot& e = base.entities[ec++];
            e.id = s.clientId;
            e.kind = 0;
            e.team = s.team;
            e.alive = s.player.alive ? 1 : 0;
            e.health = static_cast<uint8_t>(s.player.health);
            e.kills = static_cast<uint8_t>(std::min(s.kills, 255));
            e.x = s.player.position.x;
            e.y = s.player.position.y;
            e.z = s.player.position.z;
            e.yaw = s.player.yaw;
            e.pitch = s.player.pitch;
        }
        for (size_t i = 0; i < bots.size() && ec < MAX_PLAYERS + MAX_BOTS; ++i) {
            EntitySnapshot& e = base.entities[ec++];
            e.id = static_cast<uint8_t>(100 + i);
            e.kind = 1;
            e.team = 2; // unaffiliated
            e.alive = bots[i].alive ? 1 : 0;
            e.health = static_cast<uint8_t>(bots[i].health);
            e.kills = 0;
            e.x = bots[i].position.x;
            e.y = bots[i].position.y;
            e.z = bots[i].position.z;
            e.yaw = bots[i].yaw;
            e.pitch = bots[i].pitch;
        }
        base.entityCount = ec;

        uint8_t sc = 0;
        for (const SmokeVolume& v : smokeVols) {
            if (sc >= MAX_SMOKES) break;
            SmokeSnapshot& sm = base.smokes[sc++];
            sm.x = v.center.x; sm.y = v.center.y; sm.z = v.center.z;
            sm.radius = v.radius;
            sm.age = v.age;
            sm.lifetime = v.lifetime;
        }
        base.smokeCount = sc;

        // Customize per-client fields (yourId + ackSequence) and send.
        for (ConnectedClient* c : server.clients()) {
            auto it = sessions.find(c->clientId);
            if (it == sessions.end()) continue;
            SnapshotPacket snap = base;
            snap.yourId = c->clientId;
            snap.ackSequence = c->lastAck;
            server.sendSnapshotTo(*c, snap);
        }

        nextTick += std::chrono::microseconds(static_cast<long long>(DT * 1e6));
        std::this_thread::sleep_until(nextTick);
    }

    server.close();
    NetServer::globalShutdown();
    return 0;
}
