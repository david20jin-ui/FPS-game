#pragma once

#include "../core/Math.h"
#include "../weapons/Weapon.h"

#include <raylib.h>
#include <vector>

namespace fps {

struct Map;
struct SmokeVolume;
class Player;

enum class BotState { Idle, Patrol, Chase, Engage, Reload, Dead };
enum class BotDifficulty { Easy, Normal, Hard };

struct Bot {
    int     id = 0;
    Vector3 position{0, 1, 0};
    Vector3 velocity{0, 0, 0};
    float   yaw   = 0.0f;
    float   pitch = 0.0f;
    int     health = 100;
    int     armor  = 25;
    bool    alive = true;
    BotState state = BotState::Patrol;
    BotDifficulty difficulty = BotDifficulty::Normal;

    Weapon weapon = Weapon::makeClassic();

    std::vector<int> path;
    size_t pathIdx = 0;
    float repathTimer = 0.0f;

    float reactionTimer = 0.0f;
    float aimJitterTimer = 0.0f;
    Vector3 aimNoise{0, 0, 0};
    Vector3 lastSeenPlayer{};
    bool    seesPlayer = false;

    float radius() const { return 0.35f; }
    float height() const { return 1.7f; }
    Vector3 eyePos() const { return Vector3{position.x, position.y + height() - 0.2f, position.z}; }
    Vector3 chestPos() const { return Vector3{position.x, position.y + height() * 0.6f, position.z}; }
    Vector3 headPos() const { return Vector3{position.x, position.y + height() - 0.15f, position.z}; }

    void applyDamage(int dmg);
};

struct BotShot {
    int  botId;
    int  damage;
    bool headshot;
};

void updateBot(Bot& bot, Player& player, const Map& map,
               const std::vector<SmokeVolume>& smokes,
               float dt, std::vector<BotShot>& outShots);

} // namespace fps
