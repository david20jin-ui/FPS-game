#pragma once

#include <raylib.h>
#include <vector>

namespace fps {

struct Weapon;

// Small spinning brass casing ejected from the receiver on each shot.
struct BrassCasing {
    Vector3 pos{};
    Vector3 vel{};
    float   life     = 0.0f;  // seconds since spawn
    float   rotation = 0.0f;  // radians, advances by rotSpeed * dt
    float   rotSpeed = 0.0f;
};

// Persistent per-frame state for the first-person viewmodel (the visible hand
// and gun at the bottom-right of the screen).
struct ViewModelState {
    float kickLife       = 0.0f; // 0..1, decays after a shot to animate recoil
    float flashLife      = 0.0f; // 0..1, muzzle flash brightness; decays fast
    float bobPhase       = 0.0f; // advances with movement to drive walk bob
    float bobAmount      = 0.0f; // 0..1, eased toward target from player speed
    float swapBlend      = 1.0f; // 0..1, eases to 1 after a weapon swap
    int   lastWeaponSlot = -1;
    std::vector<BrassCasing> casings;
};

void tickViewModel(ViewModelState& s, float dt, float horizontalSpeed,
                   bool firedThisTick, int weaponSlot);

// Render the viewmodel using its own fixed camera. Call AFTER the main
// `EndMode3D()` for the world scene but BEFORE `drawHud()`.
void drawViewModel(const ViewModelState& s, const Weapon& weapon);

} // namespace fps
