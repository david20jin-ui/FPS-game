// Headless authoritative game server.
#include "ai/Bot.h"
#include "core/Cli.h"
#include "net/NetServer.h"
#include "player/Player.h"
#include "weapons/Smoke.h"
#include "weapons/Weapon.h"
#include "world/Map.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace fps;
using namespace fps::net;

namespace {

struct ServerSession {
    uint8_t  clientId;
    Player   player;
    std::vector<Weapon> loadout;
    Weapon   smoke;
    int      currentWeapon = 0;
};

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    Config cfg = parseCli(argc, argv);
    uint16_t port = static_cast<uint16_t>(cfg.serverPort == 0 ? 7777 : cfg.serverPort);

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
    std::printf("[fps_server] listening on :%u (mode=%s)\n", port, cfg.mode.c_str());

    Map world = Map::buildDefault();

    std::vector<Bot> bots;
    int id = 0;
    const BotDifficulty diffs[] = {BotDifficulty::Easy, BotDifficulty::Normal, BotDifficulty::Hard};
    for (size_t i = 0; i < world.botSpawns.size(); ++i) {
        Bot b{};
        b.id = id++;
        b.position = world.botSpawns[i];
        b.difficulty = diffs[i % 3];
        bots.push_back(b);
    }

    std::vector<SmokeProjectile> smokeProjs;
    std::vector<SmokeVolume>     smokeVols;

    std::unordered_map<uint8_t, ServerSession> sessions;
    auto ensureSession = [&](ConnectedClient& c) -> ServerSession& {
        auto it = sessions.find(c.clientId);
        if (it != sessions.end()) return it->second;
        ServerSession s{};
        s.clientId = c.clientId;
        s.player.spawn(world.playerSpawn);
        s.loadout = {Weapon::makeVandal(), Weapon::makeClassic(), Weapon::makeKnife()};
        s.smoke = Weapon::makeSmoke();
        return sessions.emplace(c.clientId, std::move(s)).first->second;
    };

    constexpr int    TICK_RATE = 64;
    constexpr double DT = 1.0 / TICK_RATE;
    uint32_t tick = 0;

    auto nextTick = std::chrono::steady_clock::now();
    while (true) {
        server.poll();

        for (ConnectedClient* c : server.clients()) {
            ServerSession& s = ensureSession(*c);
            while (!c->pending.empty()) {
                PlayerInput in = c->pending.front();
                c->pending.pop_front();

                s.player.yaw   = in.yaw;
                s.player.pitch = in.pitch;
                s.player.crouching = (in.buttons & 0x02) != 0;
                s.player.walking   = (in.buttons & 0x04) != 0;

                Vector3 fwd = s.player.forward(); fwd.y = 0; fwd = v3Norm(fwd);
                Vector3 right{std::cos(s.player.yaw), 0.0f, -std::sin(s.player.yaw)};
                Vector3 wish = v3Zero();
                wish = v3Add(wish, v3Scale(fwd,   static_cast<float>(in.moveForward)));
                wish = v3Add(wish, v3Scale(right, static_cast<float>(in.moveRight)));
                float wl = v3Len(wish);
                if (wl > 0) wish = v3Scale(wish, 1.0f / wl);
                float targetSpeed = s.player.speedMultiplier();
                float wishSpeed = wl > 0 ? targetSpeed : 0.0f;
                float accel = s.player.onGround ? 80.0f : 25.0f;
                float currentSpeed = v3Dot(s.player.velocity, wish);
                float addSpeed = wishSpeed - currentSpeed;
                if (addSpeed > 0 && wl > 0) {
                    float as = std::min(accel * in.dt * wishSpeed, addSpeed);
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

        Player* target = nullptr;
        for (auto& kv : sessions) {
            if (kv.second.player.alive) { target = &kv.second.player; break; }
        }
        std::vector<BotShot> shots;
        if (target) {
            for (Bot& b : bots) updateBot(b, *target, world, smokeVols, static_cast<float>(DT), shots);
            for (const BotShot& s : shots) target->applyDamage(s.damage);
        }
        tickSmokes(smokeProjs, smokeVols, world, static_cast<float>(DT));

        SnapshotPacket snap{};
        snap.tick = tick++;
        uint8_t ec = 0;
        for (auto& kv : sessions) {
            if (ec >= MAX_PLAYERS) break;
            const Player& p = kv.second.player;
            EntitySnapshot& e = snap.entities[ec++];
            e.id = kv.first;
            e.kind = 0;
            e.alive = p.alive ? 1 : 0;
            e.health = static_cast<uint8_t>(p.health);
            e.x = p.position.x; e.y = p.position.y; e.z = p.position.z;
            e.yaw = p.yaw; e.pitch = p.pitch;
        }
        for (size_t i = 0; i < bots.size() && ec < MAX_PLAYERS + MAX_BOTS; ++i) {
            EntitySnapshot& e = snap.entities[ec++];
            e.id = static_cast<uint8_t>(100 + i);
            e.kind = 1;
            e.alive = bots[i].alive ? 1 : 0;
            e.health = static_cast<uint8_t>(bots[i].health);
            e.x = bots[i].position.x; e.y = bots[i].position.y; e.z = bots[i].position.z;
            e.yaw = bots[i].yaw; e.pitch = bots[i].pitch;
        }
        snap.entityCount = ec;

        uint8_t sc = 0;
        for (const SmokeVolume& v : smokeVols) {
            if (sc >= MAX_SMOKES) break;
            SmokeSnapshot& sm = snap.smokes[sc++];
            sm.x = v.center.x; sm.y = v.center.y; sm.z = v.center.z;
            sm.radius = v.radius;
            sm.age = v.age;
            sm.lifetime = v.lifetime;
        }
        snap.smokeCount = sc;

        for (ConnectedClient* c : server.clients()) {
            snap.yourId = c->clientId;
            snap.ackSequence = c->lastAck;
            (void)c;
        }
        server.broadcastSnapshot(snap);

        nextTick += std::chrono::microseconds(static_cast<long long>(DT * 1e6));
        std::this_thread::sleep_until(nextTick);
    }

    server.close();
    NetServer::globalShutdown();
    return 0;
}
