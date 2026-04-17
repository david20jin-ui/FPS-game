#include "Map.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace fps {

namespace {

Wall makeWall(Vector3 center, Vector3 halfExtents, Color c, bool floor = false) {
    Wall w{};
    w.box.min = v3Sub(center, halfExtents);
    w.box.max = v3Add(center, halfExtents);
    w.color   = c;
    w.isFloor = floor;
    return w;
}

bool rayAabb(Vector3 origin, Vector3 dir, AABB box, float maxDist, float& outT) {
    float tmin = 0.0f;
    float tmax = maxDist;
    const float eps = 1e-8f;
    for (int i = 0; i < 3; ++i) {
        float o = (&origin.x)[i];
        float d = (&dir.x)[i];
        float lo = (&box.min.x)[i];
        float hi = (&box.max.x)[i];
        if (std::fabs(d) < eps) {
            if (o < lo || o > hi) return false;
        } else {
            float inv = 1.0f / d;
            float t0  = (lo - o) * inv;
            float t1  = (hi - o) * inv;
            if (t0 > t1) std::swap(t0, t1);
            if (t0 > tmin) tmin = t0;
            if (t1 < tmax) tmax = t1;
            if (tmin > tmax) return false;
        }
    }
    outT = tmin;
    return true;
}

bool overlapAabb(AABB a, AABB b) {
    return !(a.max.x <= b.min.x || a.min.x >= b.max.x ||
             a.max.y <= b.min.y || a.min.y >= b.max.y ||
             a.max.z <= b.min.z || a.min.z >= b.max.z);
}

} // namespace

Map Map::buildDefault() {
    Map m{};
    const Color floorCol{32, 36, 48, 255};
    const Color wallCol{64, 72, 96, 255};
    const Color coverCol{128, 80, 48, 255};
    const Color crateCol{96, 60, 32, 255};

    m.walls.push_back(makeWall({0, -0.5f, 0}, {30.0f, 0.5f, 20.0f}, floorCol, true));

    m.walls.push_back(makeWall({0,  2.0f, -20.0f}, {30.0f, 2.0f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({0,  2.0f,  20.0f}, {30.0f, 2.0f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({-30.0f, 2.0f, 0}, {0.5f, 2.0f, 20.0f}, wallCol));
    m.walls.push_back(makeWall({ 30.0f, 2.0f, 0}, {0.5f, 2.0f, 20.0f}, wallCol));

    m.walls.push_back(makeWall({0,  0.75f, 0}, {2.0f, 0.75f, 1.0f}, crateCol));
    m.walls.push_back(makeWall({ 4.0f, 0.5f, -3.0f}, {1.0f, 0.5f, 1.0f}, coverCol));
    m.walls.push_back(makeWall({-4.0f, 0.5f, -3.0f}, {1.0f, 0.5f, 1.0f}, coverCol));
    m.walls.push_back(makeWall({ 4.0f, 0.5f,  3.0f}, {1.0f, 0.5f, 1.0f}, coverCol));
    m.walls.push_back(makeWall({-4.0f, 0.5f,  3.0f}, {1.0f, 0.5f, 1.0f}, coverCol));

    m.walls.push_back(makeWall({ 10.0f, 1.5f,  8.0f}, {0.5f, 1.5f, 4.0f}, wallCol));
    m.walls.push_back(makeWall({-10.0f, 1.5f,  8.0f}, {0.5f, 1.5f, 4.0f}, wallCol));
    m.walls.push_back(makeWall({ 10.0f, 1.5f, -8.0f}, {0.5f, 1.5f, 4.0f}, wallCol));
    m.walls.push_back(makeWall({-10.0f, 1.5f, -8.0f}, {0.5f, 1.5f, 4.0f}, wallCol));

    m.playerSpawn = Vector3{0.0f, 1.0f, -17.0f};
    m.botSpawns   = {
        Vector3{  0.0f, 1.0f, 17.0f},
        Vector3{ 10.0f, 1.0f, 14.0f},
        Vector3{-10.0f, 1.0f, 14.0f},
    };

    auto addWp = [&](Vector3 p, std::initializer_list<int> nbrs) {
        NavWaypoint w{};
        w.position = p;
        for (int n : nbrs) w.neighbors.push_back(n);
        m.navGraph.push_back(w);
    };
    addWp({  0.0f, 1.0f,  17.0f}, {1, 2, 3});
    addWp({ 12.0f, 1.0f,  14.0f}, {0, 4});
    addWp({-12.0f, 1.0f,  14.0f}, {0, 5});
    addWp({  0.0f, 1.0f,   6.0f}, {0, 4, 5, 6});
    addWp({ 12.0f, 1.0f,   0.0f}, {1, 6, 7});
    addWp({-12.0f, 1.0f,   0.0f}, {2, 6, 8});
    addWp({  0.0f, 1.0f,  -6.0f}, {3, 7, 8, 9});
    addWp({ 12.0f, 1.0f, -12.0f}, {4, 9});
    addWp({-12.0f, 1.0f, -12.0f}, {5, 9});
    addWp({  0.0f, 1.0f, -17.0f}, {6, 7, 8});

    return m;
}

bool raycastMap(const Map& map, Vector3 origin, Vector3 dir, float maxDist,
                float& outDist, int& outWallIdx) {
    float best = maxDist;
    int idx = -1;
    for (size_t i = 0; i < map.walls.size(); ++i) {
        float t;
        if (rayAabb(origin, dir, map.walls[i].box, best, t) && t >= 0.0f && t < best) {
            best = t;
            idx = static_cast<int>(i);
        }
    }
    outDist = best;
    outWallIdx = idx;
    return idx >= 0;
}

void sweepPlayerAABB(const Map& map, Vector3& position, Vector3 halfExtents,
                     Vector3 velocity, float dt, bool& outOnGround) {
    outOnGround = false;
    const float axes[3] = {velocity.x * dt, velocity.y * dt, velocity.z * dt};
    for (int axis = 0; axis < 3; ++axis) {
        float move = axes[axis];
        if (move == 0.0f) continue;
        Vector3 proposed = position;
        (&proposed.x)[axis] += move;
        AABB box{};
        box.min = v3Sub(proposed, halfExtents);
        box.max = v3Add(proposed, halfExtents);
        bool blocked = false;
        for (const Wall& w : map.walls) {
            if (overlapAabb(box, w.box)) {
                blocked = true;
                if (axis == 1 && move < 0.0f) outOnGround = true;
                break;
            }
        }
        if (!blocked) position = proposed;
    }
}

} // namespace fps
