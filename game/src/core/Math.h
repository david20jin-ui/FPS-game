#pragma once

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace fps {

struct AABB {
    Vector3 min{};
    Vector3 max{};
};

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline Vector3 v3Zero() { return Vector3{0, 0, 0}; }

inline float v3Len(Vector3 v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vector3 v3Norm(Vector3 v) {
    float len = v3Len(v);
    if (len < 1e-6f) return v3Zero();
    return Vector3{v.x / len, v.y / len, v.z / len};
}

inline Vector3 v3Add(Vector3 a, Vector3 b) {
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 v3Sub(Vector3 a, Vector3 b) {
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3 v3Scale(Vector3 a, float s) {
    return Vector3{a.x * s, a.y * s, a.z * s};
}

inline float v3Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float pointRayDistance(Vector3 point, Vector3 origin, Vector3 dir) {
    Vector3 op = v3Sub(point, origin);
    float t = v3Dot(op, dir);
    if (t < 0.0f) return v3Len(op) + 1e6f;
    Vector3 closest = v3Add(origin, v3Scale(dir, t));
    return v3Len(v3Sub(point, closest));
}

} // namespace fps
