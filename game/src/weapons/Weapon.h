#pragma once

#include "../core/Math.h"
#include <raylib.h>
#include <string>

namespace fps {

enum class WeaponKind { Rifle, Pistol, Knife, Smoke };

struct WeaponStats {
    std::string name          = "Weapon";
    WeaponKind  kind          = WeaponKind::Rifle;
    float       fireInterval  = 0.1f;
    int         damageBody    = 30;
    int         damageHead    = 120;
    int         damageLeg     = 25;
    int         magSize       = 25;
    float       reloadTime    = 2.0f;
    float       baseSpread    = 0.002f;
    float       movingSpread  = 0.02f;
    float       maxRange      = 120.0f;
    bool        automatic     = true;
    float       knifeRange    = 0.0f;
    float       adsZoom       = 1.0f;
};

struct Weapon {
    WeaponStats stats{};
    int         ammoInMag   = 0;
    float       cooldown    = 0.0f;
    float       reloadLeft  = 0.0f;
    int         shotsInBurst = 0;
    float       sinceLastShot = 999.0f;

    static Weapon makeVandal();
    static Weapon makePhantom();
    static Weapon makeSpectre();
    static Weapon makeOperator();
    static Weapon makeClassic();
    static Weapon makeKnife();
    static Weapon makeSmoke();

    bool canFire() const {
        return cooldown <= 0.0f && reloadLeft <= 0.0f && ammoInMag > 0;
    }
    void startReload() {
        if (reloadLeft > 0.0f || ammoInMag == stats.magSize) return;
        reloadLeft = stats.reloadTime;
    }
    void tick(float dt) {
        cooldown = std::max(0.0f, cooldown - dt);
        sinceLastShot += dt;
        if (reloadLeft > 0.0f) {
            reloadLeft -= dt;
            if (reloadLeft <= 0.0f) {
                reloadLeft = 0.0f;
                ammoInMag = stats.magSize;
                shotsInBurst = 0;
            }
        }
        if (sinceLastShot > 0.35f) shotsInBurst = 0;
    }
};

Vector3 vandalSprayOffset(int index);

} // namespace fps
