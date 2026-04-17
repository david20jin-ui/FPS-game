#pragma once

#include "../core/Math.h"
#include <raylib.h>
#include <vector>

namespace fps {

struct Map;

struct SmokeProjectile {
    Vector3 position{};
    Vector3 velocity{};
    float   fuse = 1.4f;
    bool    alive = true;
};

struct SmokeVolume {
    Vector3 center{};
    float   radius = 4.5f;
    float   lifetime = 15.0f;
    float   age      = 0.0f;
    bool    alive() const { return age < lifetime; }
};

void tickSmokes(std::vector<SmokeProjectile>& projectiles,
                std::vector<SmokeVolume>& volumes,
                const Map& map, float dt);

bool rayIntersectsSmoke(const std::vector<SmokeVolume>& volumes,
                        Vector3 origin, Vector3 dir, float dist);

SmokeProjectile throwSmoke(Vector3 eye, Vector3 aim);

} // namespace fps
