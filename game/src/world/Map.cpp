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

namespace {

void addWp(Map& m, Vector3 p, std::initializer_list<int> nbrs) {
    NavWaypoint w{};
    w.position = p;
    for (int n : nbrs) w.neighbors.push_back(n);
    m.navGraph.push_back(w);
}

} // namespace

// ----- The Range: symmetric practice map ------------------------------------
Map Map::buildRange() {
    Map m{};
    m.displayName = "The Range";
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

    addWp(m, {  0.0f, 1.0f,  17.0f}, {1, 2, 3});
    addWp(m, { 12.0f, 1.0f,  14.0f}, {0, 4});
    addWp(m, {-12.0f, 1.0f,  14.0f}, {0, 5});
    addWp(m, {  0.0f, 1.0f,   6.0f}, {0, 4, 5, 6});
    addWp(m, { 12.0f, 1.0f,   0.0f}, {1, 6, 7});
    addWp(m, {-12.0f, 1.0f,   0.0f}, {2, 6, 8});
    addWp(m, {  0.0f, 1.0f,  -6.0f}, {3, 7, 8, 9});
    addWp(m, { 12.0f, 1.0f, -12.0f}, {4, 9});
    addWp(m, {-12.0f, 1.0f, -12.0f}, {5, 9});
    addWp(m, {  0.0f, 1.0f, -17.0f}, {6, 7, 8});

    return m;
}

// ----- Warehouse: long corridor map with pillars ----------------------------
Map Map::buildWarehouse() {
    Map m{};
    m.displayName = "Warehouse";
    const Color floorCol{28, 30, 38, 255};
    const Color wallCol{72, 78, 90, 255};
    const Color pillarCol{110, 90, 60, 255};
    const Color crateCol{90, 60, 30, 255};

    // 50 x 24 floor.
    m.walls.push_back(makeWall({0, -0.5f, 0}, {25.0f, 0.5f, 12.0f}, floorCol, true));

    // Perimeter walls (taller — 5 m).
    m.walls.push_back(makeWall({0,  2.5f, -12.0f}, {25.0f, 2.5f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({0,  2.5f,  12.0f}, {25.0f, 2.5f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({-25.0f, 2.5f, 0}, {0.5f, 2.5f, 12.0f}, wallCol));
    m.walls.push_back(makeWall({ 25.0f, 2.5f, 0}, {0.5f, 2.5f, 12.0f}, wallCol));

    // Four pillars down the center.
    for (float x : {-15.0f, -5.0f, 5.0f, 15.0f}) {
        m.walls.push_back(makeWall({x, 2.0f, 0}, {0.8f, 2.0f, 0.8f}, pillarCol));
    }
    // Crate stacks in two opposite corners for cover.
    m.walls.push_back(makeWall({-20.0f, 1.0f, -8.0f}, {2.0f, 1.0f, 2.0f}, crateCol));
    m.walls.push_back(makeWall({-18.0f, 0.5f, -5.0f}, {1.0f, 0.5f, 1.0f}, crateCol));
    m.walls.push_back(makeWall({ 20.0f, 1.0f,  8.0f}, {2.0f, 1.0f, 2.0f}, crateCol));
    m.walls.push_back(makeWall({ 18.0f, 0.5f,  5.0f}, {1.0f, 0.5f, 1.0f}, crateCol));
    // Central low divider with two gaps.
    m.walls.push_back(makeWall({-10.0f, 0.75f, 0}, {3.0f, 0.75f, 0.5f}, crateCol));
    m.walls.push_back(makeWall({ 10.0f, 0.75f, 0}, {3.0f, 0.75f, 0.5f}, crateCol));

    m.playerSpawn = Vector3{-22.0f, 1.0f, 0.0f};
    m.botSpawns   = {
        Vector3{ 22.0f, 1.0f,  0.0f},
        Vector3{ 22.0f, 1.0f,  8.0f},
        Vector3{ 22.0f, 1.0f, -8.0f},
    };

    // Waypoint grid (3 rows x 6 cols).
    addWp(m, {-22.0f, 1.0f,  0.0f}, {1, 6});
    addWp(m, {-13.0f, 1.0f,  0.0f}, {0, 2, 7});
    addWp(m, { -5.0f, 1.0f,  0.0f}, {1, 3});
    addWp(m, {  5.0f, 1.0f,  0.0f}, {2, 4});
    addWp(m, { 13.0f, 1.0f,  0.0f}, {3, 5, 10});
    addWp(m, { 22.0f, 1.0f,  0.0f}, {4, 11});
    addWp(m, {-22.0f, 1.0f,  8.0f}, {0, 7});
    addWp(m, {-13.0f, 1.0f,  8.0f}, {1, 6, 8});
    addWp(m, {  0.0f, 1.0f,  8.0f}, {7, 9});
    addWp(m, { 13.0f, 1.0f,  8.0f}, {8, 10});
    addWp(m, { 22.0f, 1.0f,  8.0f}, {4, 9, 5});
    addWp(m, { 22.0f, 1.0f, -8.0f}, {5, 12});
    addWp(m, { 13.0f, 1.0f, -8.0f}, {11, 13});
    addWp(m, {  0.0f, 1.0f, -8.0f}, {12, 14});
    addWp(m, {-13.0f, 1.0f, -8.0f}, {13, 15});
    addWp(m, {-22.0f, 1.0f, -8.0f}, {14, 0});

    return m;
}

// ----- Arena: circular-ish open map with central pit -----------------------
Map Map::buildArena() {
    Map m{};
    m.displayName = "Arena";
    const Color floorCol{36, 32, 44, 255};
    const Color wallCol{90, 70, 110, 255};
    const Color accentCol{200, 120, 60, 255};
    const Color crateCol{110, 70, 40, 255};

    // 40x40 floor.
    m.walls.push_back(makeWall({0, -0.5f, 0}, {20.0f, 0.5f, 20.0f}, floorCol, true));

    // Perimeter (shorter walls so fights stay vertical-friendly).
    m.walls.push_back(makeWall({0,  1.5f, -20.0f}, {20.0f, 1.5f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({0,  1.5f,  20.0f}, {20.0f, 1.5f, 0.5f}, wallCol));
    m.walls.push_back(makeWall({-20.0f, 1.5f, 0}, {0.5f, 1.5f, 20.0f}, wallCol));
    m.walls.push_back(makeWall({ 20.0f, 1.5f, 0}, {0.5f, 1.5f, 20.0f}, wallCol));

    // Central raised platform (cover from all sides).
    m.walls.push_back(makeWall({0, 1.0f, 0}, {3.5f, 1.0f, 3.5f}, accentCol));

    // Four diagonal cover crates.
    m.walls.push_back(makeWall({  8.0f, 0.75f,  8.0f}, {1.5f, 0.75f, 1.5f}, crateCol));
    m.walls.push_back(makeWall({ -8.0f, 0.75f,  8.0f}, {1.5f, 0.75f, 1.5f}, crateCol));
    m.walls.push_back(makeWall({  8.0f, 0.75f, -8.0f}, {1.5f, 0.75f, 1.5f}, crateCol));
    m.walls.push_back(makeWall({ -8.0f, 0.75f, -8.0f}, {1.5f, 0.75f, 1.5f}, crateCol));

    // Mid-range half-walls.
    m.walls.push_back(makeWall({  0.0f, 0.75f,  12.0f}, {4.0f, 0.75f, 0.4f}, crateCol));
    m.walls.push_back(makeWall({  0.0f, 0.75f, -12.0f}, {4.0f, 0.75f, 0.4f}, crateCol));
    m.walls.push_back(makeWall({ 12.0f, 0.75f,   0.0f}, {0.4f, 0.75f, 4.0f}, crateCol));
    m.walls.push_back(makeWall({-12.0f, 0.75f,   0.0f}, {0.4f, 0.75f, 4.0f}, crateCol));

    m.playerSpawn = Vector3{0.0f, 1.0f, -17.0f};
    m.botSpawns   = {
        Vector3{  0.0f, 1.0f, 17.0f},
        Vector3{ 15.0f, 1.0f,  0.0f},
        Vector3{-15.0f, 1.0f,  0.0f},
    };

    // 8-point perimeter ring + 4 inner nodes.
    addWp(m, {  0.0f, 1.0f,  17.0f}, {1, 7, 8});
    addWp(m, { 12.0f, 1.0f,  15.0f}, {0, 2, 8});
    addWp(m, { 17.0f, 1.0f,   0.0f}, {1, 3, 9});
    addWp(m, { 12.0f, 1.0f, -15.0f}, {2, 4, 10});
    addWp(m, {  0.0f, 1.0f, -17.0f}, {3, 5, 10});
    addWp(m, {-12.0f, 1.0f, -15.0f}, {4, 6, 10});
    addWp(m, {-17.0f, 1.0f,   0.0f}, {5, 7, 11});
    addWp(m, {-12.0f, 1.0f,  15.0f}, {0, 6, 8});
    addWp(m, {  6.0f, 1.0f,   6.0f}, {0, 1, 7, 9, 11});
    addWp(m, {  6.0f, 1.0f,  -6.0f}, {2, 3, 8, 10});
    addWp(m, { -6.0f, 1.0f,  -6.0f}, {3, 4, 5, 9, 11});
    addWp(m, { -6.0f, 1.0f,   6.0f}, {6, 7, 8, 10});

    return m;
}

Map Map::buildDefault() { return buildRange(); }

Map Map::buildByName(const std::string& name) {
    if (name == "warehouse") return buildWarehouse();
    if (name == "arena")     return buildArena();
    return buildRange();
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
