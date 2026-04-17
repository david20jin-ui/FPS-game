#include "Smoke.h"

#include "../world/Map.h"

#include <algorithm>

namespace fps {

namespace {
constexpr float GRENADE_GRAVITY = 18.0f;
constexpr float BOUNCE_DAMP     = 0.35f;
constexpr float THROW_SPEED     = 14.0f;
}

SmokeProjectile throwSmoke(Vector3 eye, Vector3 aim) {
    SmokeProjectile p{};
    p.position = eye;
    Vector3 dir = v3Norm(aim);
    dir.y += 0.2f;
    dir = v3Norm(dir);
    p.velocity = v3Scale(dir, THROW_SPEED);
    p.fuse = 1.4f;
    p.alive = true;
    return p;
}

void tickSmokes(std::vector<SmokeProjectile>& projectiles,
                std::vector<SmokeVolume>& volumes,
                const Map& map, float dt) {
    for (SmokeProjectile& p : projectiles) {
        if (!p.alive) continue;
        p.fuse -= dt;

        p.velocity.y -= GRENADE_GRAVITY * dt;
        Vector3 newPos = v3Add(p.position, v3Scale(p.velocity, dt));

        if (newPos.y < 0.2f) {
            newPos.y = 0.2f;
            p.velocity.y = -p.velocity.y * (1.0f - BOUNCE_DAMP);
            p.velocity.x *= (1.0f - BOUNCE_DAMP);
            p.velocity.z *= (1.0f - BOUNCE_DAMP);
        }

        float dist;
        int wallIdx;
        Vector3 delta = v3Sub(newPos, p.position);
        float moveLen = v3Len(delta);
        if (moveLen > 1e-4f) {
            Vector3 dir = v3Scale(delta, 1.0f / moveLen);
            if (raycastMap(map, p.position, dir, moveLen, dist, wallIdx)) {
                newPos = v3Add(p.position, v3Scale(dir, std::max(0.0f, dist - 0.1f)));
                const AABB& box = map.walls[wallIdx].box;
                Vector3 hit = newPos;
                float dx = std::min(std::fabs(hit.x - box.min.x), std::fabs(hit.x - box.max.x));
                float dy = std::min(std::fabs(hit.y - box.min.y), std::fabs(hit.y - box.max.y));
                float dz = std::min(std::fabs(hit.z - box.min.z), std::fabs(hit.z - box.max.z));
                if (dx < dy && dx < dz) p.velocity.x = -p.velocity.x * (1.0f - BOUNCE_DAMP);
                else if (dz < dx && dz < dy) p.velocity.z = -p.velocity.z * (1.0f - BOUNCE_DAMP);
                else                         p.velocity.y = -p.velocity.y * (1.0f - BOUNCE_DAMP);
            }
        }

        p.position = newPos;
        if (p.fuse <= 0.0f) {
            SmokeVolume v{};
            v.center = p.position;
            v.radius = 4.5f;
            v.lifetime = 15.0f;
            v.age = 0.0f;
            volumes.push_back(v);
            p.alive = false;
        }
    }
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const SmokeProjectile& p) { return !p.alive; }),
        projectiles.end());

    for (SmokeVolume& v : volumes) v.age += dt;
    volumes.erase(
        std::remove_if(volumes.begin(), volumes.end(),
                       [](const SmokeVolume& v) { return !v.alive(); }),
        volumes.end());
}

bool rayIntersectsSmoke(const std::vector<SmokeVolume>& volumes,
                        Vector3 origin, Vector3 dir, float dist) {
    for (const SmokeVolume& v : volumes) {
        Vector3 oc = v3Sub(v.center, origin);
        float t  = v3Dot(oc, dir);
        if (t < 0.0f) continue;
        if (t > dist + v.radius) continue;
        float tt = clampf(t, 0.0f, dist);
        Vector3 closest = v3Add(origin, v3Scale(dir, tt));
        float d = v3Len(v3Sub(closest, v.center));
        if (d < v.radius * 0.95f) return true;
    }
    return false;
}

} // namespace fps
