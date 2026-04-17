#include "Player.h"

#include "../world/Map.h"

namespace fps {

namespace {
constexpr float GRAVITY          = 22.0f;
constexpr float GROUND_FRICTION  = 14.0f;

bool overlap(AABB a, AABB b) {
    return !(a.max.x <= b.min.x || a.min.x >= b.max.x ||
             a.max.y <= b.min.y || a.min.y >= b.max.y ||
             a.max.z <= b.min.z || a.min.z >= b.max.z);
}

AABB playerBox(Vector3 position, float radius, float height) {
    AABB b{};
    b.min = Vector3{position.x - radius, position.y,          position.z - radius};
    b.max = Vector3{position.x + radius, position.y + height, position.z + radius};
    return b;
}

bool collidesWithMap(const Map& map, AABB box) {
    for (const Wall& w : map.walls) if (overlap(box, w.box)) return true;
    return false;
}
}

void Player::spawn(Vector3 pos) {
    position = pos;
    velocity = v3Zero();
    yaw = 0; pitch = 0;
    health = 100; armor = 50;
    alive = true; onGround = false;
    crouching = false; walking = false;
}

void Player::handleMouseLook(float dx, float dy) {
    constexpr float BASE = 0.0022f;
    yaw   -= dx * BASE * sensitivity;
    pitch -= dy * BASE * sensitivity;
    const float limit = 1.55f;
    pitch = clampf(pitch, -limit, limit);
}

Vector3 Player::forward() const {
    return Vector3{
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::cos(yaw),
    };
}

Vector3 Player::right() const {
    return Vector3{std::cos(yaw), 0.0f, -std::sin(yaw)};
}

Vector3 Player::eyePosition() const {
    Vector3 p = position;
    p.y += currentHeight() - 0.2f;
    return p;
}

Camera3D Player::buildCamera() const {
    Camera3D cam{};
    cam.position = eyePosition();
    cam.target = v3Add(cam.position, forward());
    cam.up = Vector3{0, 1, 0};
    cam.fovy = fovDegrees;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

AABB Player::bounds() const {
    return playerBox(position, RADIUS, currentHeight());
}

float Player::speedMultiplier() const {
    if (crouching) return 2.5f;
    if (walking)   return 3.2f;
    return 6.0f;
}

void Player::applyDamage(int dmg) {
    if (!alive) return;
    int soaked = std::min(armor, dmg / 2);
    armor -= soaked;
    dmg -= soaked;
    health -= dmg;
    if (health <= 0) { health = 0; alive = false; }
}

void Player::update(const Map& map, float dt) {
    if (!alive) { velocity = v3Zero(); return; }

    if (onGround) {
        float vx = velocity.x;
        float vz = velocity.z;
        float len = std::sqrt(vx * vx + vz * vz);
        float drop = GROUND_FRICTION * dt;
        if (len < drop) {
            velocity.x = 0; velocity.z = 0;
        } else if (len > 1e-4f) {
            float s = (len - drop) / len;
            velocity.x *= s;
            velocity.z *= s;
        }
    }

    velocity.y -= GRAVITY * dt;

    const float radius = RADIUS;
    const float height = currentHeight();

    auto trySweep = [&](int axis, float delta) -> bool {
        Vector3 proposed = position;
        (&proposed.x)[axis] += delta;
        AABB box = playerBox(proposed, radius, height);
        if (axis == 1 && proposed.y < 0.0f) return false;
        if (collidesWithMap(map, box)) return false;
        position = proposed;
        return true;
    };

    onGround = false;
    float dx = velocity.x * dt;
    if (dx != 0.0f && !trySweep(0, dx)) velocity.x = 0;
    float dz = velocity.z * dt;
    if (dz != 0.0f && !trySweep(2, dz)) velocity.z = 0;
    float dy = velocity.y * dt;
    if (dy != 0.0f) {
        if (!trySweep(1, dy)) {
            if (velocity.y < 0) onGround = true;
            velocity.y = 0;
        }
    }
    if (position.y <= 0.0f) {
        position.y = 0.0f;
        if (velocity.y < 0) velocity.y = 0;
        onGround = true;
    }
}

} // namespace fps
