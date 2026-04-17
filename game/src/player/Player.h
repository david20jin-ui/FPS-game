#pragma once

#include "../core/Math.h"

#include <raylib.h>

namespace fps {

struct Map;

class Player {
public:
    static constexpr float HEIGHT_STAND  = 1.8f;
    static constexpr float HEIGHT_CROUCH = 1.1f;
    static constexpr float RADIUS        = 0.35f;

    Vector3 position{0, 1, 0};
    Vector3 velocity{0, 0, 0};
    float   yaw   = 0.0f;
    float   pitch = 0.0f;

    int   health = 100;
    int   armor  = 50;
    bool  alive  = true;
    bool  onGround = false;
    bool  crouching = false;
    bool  walking   = false;

    float sensitivity = 0.4f;
    float fovDegrees  = 90.0f;

    void spawn(Vector3 pos);
    void update(const Map& map, float dt);
    void handleMouseLook(float dx, float dy);
    void applyDamage(int dmg);

    Vector3 eyePosition() const;
    Vector3 forward() const;
    Vector3 right() const;
    Camera3D buildCamera() const;
    AABB    bounds() const;
    float   currentHeight() const { return crouching ? HEIGHT_CROUCH : HEIGHT_STAND; }
    float   speedMultiplier() const;
};

} // namespace fps
