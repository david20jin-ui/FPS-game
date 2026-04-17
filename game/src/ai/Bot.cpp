#include "Bot.h"

#include "Nav.h"
#include "../player/Player.h"
#include "../weapons/Smoke.h"
#include "../world/Map.h"

#include <cmath>
#include <cstdlib>

namespace fps {

namespace {

float randf() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
float randRange(float a, float b) { return a + randf() * (b - a); }

struct DifficultyParams {
    float reactionTime;
    float aimError;
    float aimSnapSpeed;
    float engagementRange;
    float accuracyScale;
};

DifficultyParams difficultyOf(BotDifficulty d) {
    switch (d) {
    case BotDifficulty::Easy:   return {0.55f, 0.06f, 6.0f,   35.0f, 0.45f};
    case BotDifficulty::Normal: return {0.35f, 0.03f, 10.0f,  55.0f, 0.75f};
    case BotDifficulty::Hard:   return {0.18f, 0.012f, 18.0f, 80.0f, 0.95f};
    }
    return {0.35f, 0.03f, 10.0f, 55.0f, 0.75f};
}

bool lineOfSight(const Map& map, const std::vector<SmokeVolume>& smokes,
                 Vector3 from, Vector3 to) {
    Vector3 delta = v3Sub(to, from);
    float d = v3Len(delta);
    if (d < 1e-4f) return true;
    Vector3 dir = v3Scale(delta, 1.0f / d);
    float hitDist;
    int wallIdx;
    if (raycastMap(map, from, dir, d, hitDist, wallIdx) && hitDist < d) return false;
    if (rayIntersectsSmoke(smokes, from, dir, d)) return false;
    return true;
}

void seekYaw(Bot& bot, float targetYaw, float speed, float dt) {
    float diff = targetYaw - bot.yaw;
    while (diff >  PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    float step = clampf(diff, -speed * dt, speed * dt);
    bot.yaw += step;
}

Vector3 aimAt(const Bot& bot, Vector3 target) {
    Vector3 d = v3Sub(target, bot.eyePos());
    return v3Norm(d);
}

void moveAlongPath(Bot& bot, const Map& map, float dt) {
    if (bot.path.empty() || bot.pathIdx >= bot.path.size()) return;
    Vector3 target = map.navGraph[bot.path[bot.pathIdx]].position;
    Vector3 toTarget = v3Sub(target, bot.position);
    toTarget.y = 0;
    float d = v3Len(toTarget);
    if (d < 0.7f) { ++bot.pathIdx; return; }
    Vector3 dir = v3Scale(toTarget, 1.0f / std::max(d, 1e-4f));
    const float speed = 3.8f;
    bot.position.x += dir.x * speed * dt;
    bot.position.z += dir.z * speed * dt;
    float desiredYaw = std::atan2(dir.x, dir.z);
    seekYaw(bot, desiredYaw, 4.5f, dt);
}

} // namespace

void Bot::applyDamage(int dmg) {
    if (!alive) return;
    int soak = std::min(armor, dmg / 2);
    armor -= soak;
    dmg -= soak;
    health -= dmg;
    if (health <= 0) { health = 0; alive = false; state = BotState::Dead; }
}

void updateBot(Bot& bot, Player& player, const Map& map,
               const std::vector<SmokeVolume>& smokes,
               float dt, std::vector<BotShot>& outShots) {
    if (!bot.alive) return;

    const DifficultyParams dp = difficultyOf(bot.difficulty);
    bot.weapon.tick(dt);

    Vector3 playerChest = v3Add(player.position,
                                Vector3{0, player.currentHeight() * 0.55f, 0});
    Vector3 toPlayer = v3Sub(playerChest, bot.eyePos());
    float playerDist = v3Len(toPlayer);
    bool canSee = player.alive &&
                  playerDist <= dp.engagementRange &&
                  lineOfSight(map, smokes, bot.eyePos(), playerChest);

    if (canSee) {
        if (!bot.seesPlayer) bot.reactionTimer = dp.reactionTime;
        bot.seesPlayer = true;
        bot.lastSeenPlayer = player.position;
        bot.state = BotState::Engage;
    } else if (bot.seesPlayer) {
        bot.seesPlayer = false;
        bot.state = BotState::Chase;
    }

    if (bot.weapon.ammoInMag == 0 && bot.weapon.reloadLeft <= 0.0f) {
        bot.weapon.startReload();
    }

    bot.repathTimer -= dt;
    if (bot.state == BotState::Patrol && (bot.path.empty() || bot.pathIdx >= bot.path.size() || bot.repathTimer <= 0.0f)) {
        bot.repathTimer = randRange(2.5f, 5.0f);
        if (!map.navGraph.empty()) {
            int targetIdx = static_cast<int>(randRange(0.0f, static_cast<float>(map.navGraph.size() - 0.01f)));
            bot.path = findPath(map, bot.position, map.navGraph[targetIdx].position);
            bot.pathIdx = 0;
        }
    } else if (bot.state == BotState::Chase) {
        if (bot.path.empty() || bot.pathIdx >= bot.path.size() || bot.repathTimer <= 0.0f) {
            bot.repathTimer = 0.7f;
            bot.path = findPath(map, bot.position, bot.lastSeenPlayer);
            bot.pathIdx = 0;
        }
    } else if (bot.state == BotState::Engage) {
        float strafe = std::sin(GetTime() * 2.0f + bot.id) * 0.6f;
        Vector3 rightVec{std::cos(bot.yaw), 0, -std::sin(bot.yaw)};
        bot.position.x += rightVec.x * strafe * dt;
        bot.position.z += rightVec.z * strafe * dt;
    }

    if (bot.state == BotState::Patrol || bot.state == BotState::Chase) {
        moveAlongPath(bot, map, dt);
    }

    if (bot.position.y < 0.0f) bot.position.y = 0.0f;

    if (bot.state == BotState::Engage) {
        Vector3 aimDir = aimAt(bot, playerChest);
        float targetYaw = std::atan2(aimDir.x, aimDir.z);
        float targetPitch = std::asin(clampf(aimDir.y, -1.0f, 1.0f));
        seekYaw(bot, targetYaw, dp.aimSnapSpeed, dt);
        bot.pitch += clampf(targetPitch - bot.pitch, -dp.aimSnapSpeed * dt, dp.aimSnapSpeed * dt);

        bot.reactionTimer -= dt;
        if (bot.reactionTimer <= 0.0f && bot.weapon.canFire()) {
            float rangePenalty = clampf(1.0f - (playerDist / dp.engagementRange), 0.0f, 1.0f);
            float hitChance = dp.accuracyScale * rangePenalty;
            float playerSpeed = v3Len(Vector3{player.velocity.x, 0, player.velocity.z});
            hitChance *= clampf(1.0f - playerSpeed * 0.05f, 0.25f, 1.0f);

            bot.weapon.ammoInMag--;
            bot.weapon.cooldown = bot.weapon.stats.fireInterval;
            bot.weapon.sinceLastShot = 0.0f;

            if (randf() < hitChance) {
                bool headshot = randf() < 0.18f * dp.accuracyScale;
                int dmg = headshot ? bot.weapon.stats.damageHead : bot.weapon.stats.damageBody;
                outShots.push_back(BotShot{bot.id, dmg, headshot});
            }
        }
    } else {
        if (!canSee && bot.state == BotState::Chase) {
            if (v3Len(v3Sub(bot.position, bot.lastSeenPlayer)) < 2.0f) {
                bot.state = BotState::Patrol;
                bot.path.clear();
            }
        }
    }
}

} // namespace fps
