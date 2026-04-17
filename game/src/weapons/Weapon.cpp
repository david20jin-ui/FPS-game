#include "Weapon.h"

namespace fps {

Weapon Weapon::makeVandal() {
    Weapon w{};
    w.stats.name = "Vandal";
    w.stats.kind = WeaponKind::Rifle;
    w.stats.fireInterval = 0.0923f;
    w.stats.damageBody = 39;
    w.stats.damageHead = 156;
    w.stats.damageLeg  = 33;
    w.stats.magSize = 25;
    w.stats.reloadTime = 2.5f;
    w.stats.baseSpread = 0.0008f;
    w.stats.movingSpread = 0.035f;
    w.stats.maxRange = 200.0f;
    w.stats.automatic = true;
    w.stats.adsZoom = 1.25f;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Weapon Weapon::makePhantom() {
    Weapon w{};
    w.stats.name = "Phantom";
    w.stats.kind = WeaponKind::Rifle;
    w.stats.fireInterval = 0.0923f;
    w.stats.damageBody = 39;
    w.stats.damageHead = 140;
    w.stats.damageLeg  = 33;
    w.stats.magSize = 30;
    w.stats.reloadTime = 2.5f;
    w.stats.baseSpread = 0.0009f;
    w.stats.movingSpread = 0.032f;
    w.stats.maxRange = 150.0f;
    w.stats.automatic = true;
    w.stats.adsZoom = 1.15f;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Weapon Weapon::makeSpectre() {
    Weapon w{};
    w.stats.name = "Spectre";
    w.stats.kind = WeaponKind::Rifle;
    w.stats.fireInterval = 0.075f;
    w.stats.damageBody = 22;
    w.stats.damageHead = 78;
    w.stats.damageLeg  = 19;
    w.stats.magSize = 30;
    w.stats.reloadTime = 2.25f;
    w.stats.baseSpread = 0.004f;
    w.stats.movingSpread = 0.02f;
    w.stats.maxRange = 120.0f;
    w.stats.automatic = true;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Weapon Weapon::makeOperator() {
    Weapon w{};
    w.stats.name = "Operator";
    w.stats.kind = WeaponKind::Rifle;
    w.stats.fireInterval = 1.5f;
    w.stats.damageBody = 150;
    w.stats.damageHead = 255;
    w.stats.damageLeg  = 120;
    w.stats.magSize = 5;
    w.stats.reloadTime = 3.7f;
    w.stats.baseSpread = 0.0f;
    w.stats.movingSpread = 0.12f;
    w.stats.maxRange = 300.0f;
    w.stats.automatic = false;
    w.stats.adsZoom = 2.5f;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Weapon Weapon::makeClassic() {
    Weapon w{};
    w.stats.name = "Classic";
    w.stats.kind = WeaponKind::Pistol;
    w.stats.fireInterval = 0.15f;
    w.stats.damageBody = 26;
    w.stats.damageHead = 78;
    w.stats.damageLeg  = 22;
    w.stats.magSize = 12;
    w.stats.reloadTime = 1.75f;
    w.stats.baseSpread = 0.003f;
    w.stats.movingSpread = 0.04f;
    w.stats.maxRange = 80.0f;
    w.stats.automatic = false;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Weapon Weapon::makeKnife() {
    Weapon w{};
    w.stats.name = "Knife";
    w.stats.kind = WeaponKind::Knife;
    w.stats.fireInterval = 0.65f;
    w.stats.damageBody = 50;
    w.stats.damageHead = 75;
    w.stats.damageLeg  = 50;
    w.stats.magSize = 1;
    w.stats.reloadTime = 0.0f;
    w.stats.baseSpread = 0.0f;
    w.stats.movingSpread = 0.0f;
    w.stats.maxRange = 0.0f;
    w.stats.knifeRange = 2.2f;
    w.stats.automatic = false;
    w.ammoInMag = 1;
    return w;
}

Weapon Weapon::makeSmoke() {
    Weapon w{};
    w.stats.name = "Smoke";
    w.stats.kind = WeaponKind::Smoke;
    w.stats.fireInterval = 0.9f;
    w.stats.damageBody = 0;
    w.stats.magSize = 2;
    w.stats.reloadTime = 0.0f;
    w.ammoInMag = w.stats.magSize;
    return w;
}

Vector3 vandalSprayOffset(int index) {
    static const Vector2 pattern[] = {
        {0.000f, 0.000f}, {0.000f, 0.010f}, {0.001f, 0.020f}, {0.002f, 0.028f},
        {0.004f, 0.034f}, {0.006f, 0.038f}, {0.008f, 0.040f}, {0.010f, 0.042f},
        {0.013f, 0.042f}, {0.009f, 0.041f}, {0.002f, 0.040f}, {-0.005f, 0.038f},
        {-0.010f, 0.036f}, {-0.013f, 0.034f}, {-0.014f, 0.032f}, {-0.012f, 0.031f},
        {-0.008f, 0.031f}, {-0.002f, 0.031f}, {0.004f, 0.031f}, {0.010f, 0.032f},
        {0.015f, 0.033f}, {0.017f, 0.034f}, {0.016f, 0.035f}, {0.013f, 0.035f},
        {0.008f, 0.035f},
    };
    const int N = static_cast<int>(sizeof(pattern) / sizeof(pattern[0]));
    const Vector2& p = pattern[index % N];
    return Vector3{p.x, p.y, 0.0f};
}

} // namespace fps
