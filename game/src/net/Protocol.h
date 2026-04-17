#pragma once

#include "../sim/Input.h"

#include <cstdint>

namespace fps::net {

constexpr uint16_t PROTOCOL_VERSION = 2;
constexpr int      MAX_PLAYERS = 10;
constexpr int      MAX_BOTS    = 16;
constexpr int      MAX_SMOKES  = 8;

enum MsgId : uint8_t {
    C2S_Hello       = 1,
    S2C_Welcome     = 2,
    C2S_Input       = 3,
    S2C_Snapshot    = 4,
    S2C_Bye         = 5,
    C2S_Fire        = 6,
    S2C_HitFeedback = 7,
};

#pragma pack(push, 1)

struct HelloPacket {
    uint8_t  id = C2S_Hello;
    uint16_t version = PROTOCOL_VERSION;
    char     name[32]{};
};

struct WelcomePacket {
    uint8_t  id = S2C_Welcome;
    uint16_t version = PROTOCOL_VERSION;
    uint8_t  clientId = 0;
    uint8_t  team     = 0; // 0 = blue, 1 = red
    uint32_t mapSeed  = 0;
};

struct InputPacket {
    uint8_t     id = C2S_Input;
    PlayerInput input{};
};

// Client -> Server: "I pressed fire now, here's my aim."
// The server reconstructs the hitscan against its authoritative world.
struct FirePacket {
    uint8_t  id = C2S_Fire;
    uint32_t sequence = 0;
    uint8_t  weaponSlot = 0; // client's loadout index (purely informational)
    uint8_t  weaponKind = 0; // 0 rifle, 1 pistol, 2 knife
    float    originX = 0, originY = 0, originZ = 0;
    float    dirX = 0, dirY = 0, dirZ = 1;
    float    maxRange = 100.0f;
    uint8_t  headshotDamage = 156; // server clamps per weapon config
    uint8_t  bodyDamage     = 39;
};

// Per-entity snapshot sent in the broadcast update.
struct EntitySnapshot {
    uint8_t id;       // clientId (0..) for players, 100+idx for bots
    uint8_t kind;     // 0 = player, 1 = bot
    uint8_t team;     // 0 blue, 1 red, 2 unaffiliated (bots)
    uint8_t alive;
    uint8_t health;
    uint8_t kills;    // kills this match (scoreboard)
    float   x, y, z;
    float   yaw;
    float   pitch;
};

struct SmokeSnapshot {
    float x, y, z;
    float radius;
    float age;
    float lifetime;
};

struct SnapshotPacket {
    uint8_t  id = S2C_Snapshot;
    uint32_t tick = 0;
    uint32_t ackSequence = 0;
    uint8_t  yourId = 0;
    uint8_t  entityCount = 0;
    uint8_t  smokeCount = 0;
    uint8_t  scoreBlue = 0;   // team totals
    uint8_t  scoreRed  = 0;
    EntitySnapshot entities[MAX_PLAYERS + MAX_BOTS];
    SmokeSnapshot  smokes[MAX_SMOKES];
};

// Server -> Client: your fire landed on someone. Sent to the shooter only.
struct HitFeedbackPacket {
    uint8_t  id = S2C_HitFeedback;
    uint8_t  targetId = 0;
    uint8_t  damage = 0;
    uint8_t  headshot = 0;
    uint8_t  killed = 0;
};

#pragma pack(pop)

} // namespace fps::net
