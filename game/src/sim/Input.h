#pragma once

#include <cstdint>

namespace fps {

struct PlayerInput {
    uint32_t sequence = 0;
    float    dt       = 0.0f;
    float    yaw      = 0.0f;
    float    pitch    = 0.0f;
    int8_t   moveForward = 0;
    int8_t   moveRight   = 0;
    uint8_t  buttons     = 0;
    uint8_t  weaponSlot  = 0;
};

} // namespace fps
