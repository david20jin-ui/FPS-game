#pragma once

#include "../sim/Input.h"

#include <cstdint>

namespace fps::net {

constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr int      MAX_PLAYERS = 10;
constexpr int      MAX_BOTS    = 16;
constexpr int      MAX_SMOKES  = 8;

enum MsgId : uint8_t {
    C2S_Hello    = 1,
    S2C_Welcome  = 2,
    C2S_Input    = 3,
    S2C_Snapshot = 4,
    S2C_Bye      = 5,
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
    uint32_t mapSeed = 0;
};

struct InputPacket {
    uint8_t     id = C2S_Input;
    PlayerInput input{};
};

struct EntitySnapshot {
    uint8_t  id;
    uint8_t  kind;
    uint8_t  alive;
    uint8_t  health;
    float    x, y, z;
    float    yaw;
    float    pitch;
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
    EntitySnapshot entities[MAX_PLAYERS + MAX_BOTS];
    SmokeSnapshot  smokes[MAX_SMOKES];
};

#pragma pack(pop)

} // namespace fps::net
