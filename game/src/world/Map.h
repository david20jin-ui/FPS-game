#pragma once

#include "../core/Math.h"

#include <raylib.h>
#include <vector>

namespace fps {

struct Wall {
    AABB box;
    Color color;
    bool  isFloor = false;
};

struct NavWaypoint {
    Vector3 position;
    std::vector<int> neighbors;
};

struct Map {
    std::vector<Wall> walls;
    Vector3 playerSpawn{0.0f, 1.0f, 0.0f};
    std::vector<Vector3> botSpawns;
    std::vector<NavWaypoint> navGraph;

    static Map buildDefault();
};

bool raycastMap(const Map& map, Vector3 origin, Vector3 dir, float maxDist,
                float& outDist, int& outWallIdx);

void sweepPlayerAABB(const Map& map, Vector3& position, Vector3 halfExtents,
                     Vector3 velocity, float dt, bool& outOnGround);

} // namespace fps
