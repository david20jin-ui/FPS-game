#include "ViewModel.h"

#include "../core/Math.h"
#include "../weapons/Weapon.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace fps {

namespace {

// ----- Palette --------------------------------------------------------------
// Tuned to the Valorant "Prime Vandal" reference: light gray body, darker
// grips, brass accents on the chamber and magazine.
constexpr Color SKIN       {230, 190, 160, 255};
constexpr Color GLOVE      { 38,  42,  60, 255};
constexpr Color GLOVE_RIM  { 80,  95, 130, 255};

constexpr Color BODY_LIGHT {180, 185, 195, 255}; // main receiver
constexpr Color BODY_MID   {130, 135, 145, 255}; // shaded panels
constexpr Color BODY_DARK  { 55,  58,  68, 255}; // trigger housing, rails
constexpr Color BRASS      {215, 165,  85, 255};
constexpr Color BRASS_DARK {155, 115,  50, 255};
constexpr Color GUN_METAL  { 95, 100, 115, 255};
constexpr Color MAG_COLOR  { 48,  52,  64, 255};
constexpr Color BLADE      {210, 220, 235, 255};
constexpr Color SMOKE_GRN  { 80,  95,  70, 255};

// Viewmodel camera convention (gluLookAt, target=+Z, up=+Y):
//   screen-right maps to world -X, so a right-hand gun sits at NEGATIVE X.

// Roll the whole viewmodel around the view axis (screen plane) by this
// many degrees so the barrel tilts up-left, matching the reference.
constexpr float ROLL_DEGREES = 7.0f;

// ----- Reload animation ----------------------------------------------------
// A phased animation driven purely by the weapon's remaining reload time:
//   [0.00, 0.12]  Gun dips + tilts inward; mag starts to detach.
//   [0.12, 0.30]  Mag is fully out and falling; left hand leaves the grip.
//   [0.30, 0.55]  New mag rises from below and slams into the well.
//   [0.55, 0.68]  Brief pause (tap the mag seat).
//   [0.68, 0.86]  Bolt / slide pulled back then released.
//   [0.86, 1.00]  Returns to idle pose.
struct WeaponAnim {
    float dip          = 0.0f;   // 0..1, vertical drop of whole viewmodel
    float tiltExtra    = 0.0f;   // extra roll degrees
    float jostleY      = 0.0f;   // one-shot vertical bump at key moments
    bool  magAttached  = true;
    float newMagY      = 0.0f;   // 0 = inserted, -1 = fully below well
    float boltBack     = 0.0f;   // 0 = forward (idle), 1 = fully racked back
    float leftHandDrop = 0.0f;   // 0..1, how far the left hand has left the grip
    bool  showDetachedMag    = false;
    Vector3 detachedMagPos   {};
    float detachedMagAlpha   = 1.0f;
};

inline float smoothstep01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

WeaponAnim computeReloadAnim(float t, WeaponKind kind, Vector3 gripIdle) {
    WeaponAnim a{};
    const bool hasFallingMag = (kind == WeaponKind::Rifle || kind == WeaponKind::Pistol);
    const float dipFinal  = 1.0f;
    const float tiltFinal = 14.0f;

    if (t < 0.12f) {
        float f = smoothstep01(t / 0.12f);
        a.dip = f * dipFinal;
        a.tiltExtra = f * tiltFinal;
        a.magAttached = (f < 0.8f); // drops off near the end of this phase
    } else if (t < 0.30f) {
        float f = (t - 0.12f) / 0.18f;
        a.dip = dipFinal;
        a.tiltExtra = tiltFinal;
        a.magAttached = false;
        a.leftHandDrop = smoothstep01(f * 1.5f);
        if (hasFallingMag) {
            a.showDetachedMag = true;
            float fallT = f;
            a.detachedMagPos = Vector3{
                gripIdle.x + 0.015f + 0.05f * fallT,
                gripIdle.y - 0.22f - 1.6f * fallT * fallT,
                gripIdle.z + 0.02f + 0.04f * fallT,
            };
            a.detachedMagAlpha = 1.0f;
        }
    } else if (t < 0.55f) {
        float f = (t - 0.30f) / 0.25f;
        a.dip = dipFinal;
        a.tiltExtra = tiltFinal;
        a.magAttached = false;
        a.leftHandDrop = 1.0f - smoothstep01(f);
        a.newMagY = -(1.0f - smoothstep01(f));
        if (hasFallingMag) {
            a.showDetachedMag = true;
            a.detachedMagPos = Vector3{
                gripIdle.x + 0.065f + 0.05f * f,
                gripIdle.y - 0.50f - 0.45f * f,
                gripIdle.z + 0.06f + 0.04f * f,
            };
            a.detachedMagAlpha = std::max(0.0f, 1.0f - f * 1.2f);
        }
    } else if (t < 0.68f) {
        // Mag seat: gun jerks up slightly when the mag taps home.
        float f = (t - 0.55f) / 0.13f;
        a.dip = dipFinal;
        a.tiltExtra = tiltFinal;
        a.magAttached = true;
        a.jostleY = std::sin(f * PI) * 0.02f;
    } else if (t < 0.86f) {
        // Bolt/slide pull + release (sine arc: back then forward).
        float f = (t - 0.68f) / 0.18f;
        a.dip = dipFinal;
        a.tiltExtra = tiltFinal;
        a.magAttached = true;
        a.boltBack = std::sin(f * PI);
        // Small snap at the slam-forward moment.
        if (f > 0.72f) a.jostleY = -0.015f * (1.0f - (f - 0.72f) / 0.28f);
    } else {
        float f = (t - 0.86f) / 0.14f;
        float inv = 1.0f - smoothstep01(f);
        a.dip = dipFinal * inv;
        a.tiltExtra = tiltFinal * inv;
        a.magAttached = true;
    }
    return a;
}

// ----- Primitive helpers ----------------------------------------------------
inline float randf() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

inline void box(Vector3 center, Vector3 size, Color c) {
    DrawCubeV(center, size, c);
}

// ----- Weapon geometry ------------------------------------------------------
// All positions are expressed relative to the "grip" anchor (approximately
// where the shooter's right hand wraps the pistol grip).
struct Basis {
    Vector3 grip;
};

void drawRifle(Basis b, const Weapon& w, const WeaponAnim& anim) {
    const Vector3 g = b.grip;

    // ----- Stock (shoulder end) -------------------------------------------
    box({g.x - 0.005f, g.y + 0.030f, g.z - 0.18f},
        {0.060f, 0.090f, 0.230f}, BODY_LIGHT);
    // Rubber butt-pad at the back.
    box({g.x - 0.005f, g.y + 0.030f, g.z - 0.30f},
        {0.068f, 0.100f, 0.020f}, BODY_DARK);
    // Cheek rest raise on the top of the stock.
    box({g.x - 0.005f, g.y + 0.090f, g.z - 0.18f},
        {0.055f, 0.020f, 0.160f}, BODY_MID);

    // ----- Receiver / main body -------------------------------------------
    box({g.x - 0.005f, g.y + 0.040f, g.z + 0.10f},
        {0.100f, 0.100f, 0.430f}, BODY_LIGHT);
    // Dust cover / top panel (slightly raised and darker).
    box({g.x - 0.005f, g.y + 0.098f, g.z + 0.10f},
        {0.080f, 0.025f, 0.380f}, BODY_MID);
    // Brass bolt band — the iconic Vandal/Phantom chamber accent.
    box({g.x - 0.005f, g.y + 0.075f, g.z - 0.04f},
        {0.110f, 0.045f, 0.035f}, BRASS);
    box({g.x - 0.005f, g.y + 0.075f, g.z - 0.07f},
        {0.115f, 0.050f, 0.008f}, BRASS_DARK);

    // Ejection port slot (visible dark rectangle).
    box({g.x - 0.053f, g.y + 0.085f, g.z + 0.04f},
        {0.010f, 0.020f, 0.100f}, BODY_DARK);

    // ----- Iron sight rail + front/rear sights ----------------------------
    box({g.x - 0.005f, g.y + 0.128f, g.z + 0.10f},
        {0.030f, 0.030f, 0.330f}, BODY_DARK);
    box({g.x - 0.005f, g.y + 0.158f, g.z + 0.30f},
        {0.022f, 0.035f, 0.025f}, GUN_METAL); // front post
    box({g.x - 0.005f, g.y + 0.158f, g.z - 0.04f},
        {0.034f, 0.028f, 0.030f}, GUN_METAL); // rear sight

    // ----- Charging handle (slides back during the bolt-pull phase) -------
    {
        float zOffset = -anim.boltBack * 0.09f;
        // Slim dark handle protruding from the left side of the receiver.
        box({g.x - 0.058f, g.y + 0.075f, g.z + 0.18f + zOffset},
            {0.014f, 0.025f, 0.050f}, BODY_DARK);
        box({g.x - 0.070f, g.y + 0.075f, g.z + 0.18f + zOffset},
            {0.010f, 0.020f, 0.020f}, GUN_METAL);
    }

    // ----- Magazine -------------------------------------------------------
    // Seated mag (hidden during the ejection + insertion phases of reload).
    if (anim.magAttached) {
        box({g.x - 0.005f, g.y - 0.105f, g.z + 0.02f},
            {0.080f, 0.175f, 0.120f}, MAG_COLOR);
        box({g.x - 0.005f, g.y - 0.020f, g.z + 0.02f},
            {0.085f, 0.022f, 0.125f}, BRASS);
        box({g.x - 0.005f, g.y - 0.200f, g.z + 0.02f},
            {0.088f, 0.020f, 0.128f}, BODY_DARK);
    } else if (anim.newMagY < 0.0f) {
        // Fresh mag rising from below the well into position.
        float dy = anim.newMagY * 0.36f; // world-units drop while approaching
        box({g.x - 0.005f, g.y - 0.105f + dy, g.z + 0.02f},
            {0.080f, 0.175f, 0.120f}, MAG_COLOR);
        box({g.x - 0.005f, g.y - 0.020f + dy, g.z + 0.02f},
            {0.085f, 0.022f, 0.125f}, BRASS);
        box({g.x - 0.005f, g.y - 0.200f + dy, g.z + 0.02f},
            {0.088f, 0.020f, 0.128f}, BODY_DARK);
    }

    // ----- Trigger guard + trigger ----------------------------------------
    box({g.x - 0.005f, g.y - 0.015f, g.z + 0.01f},
        {0.022f, 0.045f, 0.060f}, BODY_DARK);
    box({g.x - 0.005f, g.y - 0.028f, g.z + 0.01f},
        {0.014f, 0.018f, 0.020f}, GUN_METAL);

    // ----- Hand guard (foregrip area) -------------------------------------
    box({g.x - 0.005f, g.y + 0.030f, g.z + 0.34f},
        {0.080f, 0.080f, 0.140f}, BODY_MID);
    // Ventilation slots on the handguard (dark strips).
    for (int i = 0; i < 3; ++i) {
        float z = g.z + 0.30f + i * 0.035f;
        box({g.x - 0.048f, g.y + 0.030f, z},
            {0.006f, 0.045f, 0.018f}, BODY_DARK);
    }

    // ----- Barrel + muzzle ------------------------------------------------
    box({g.x - 0.005f, g.y + 0.048f, g.z + 0.52f},
        {0.042f, 0.042f, 0.230f}, GUN_METAL);
    box({g.x - 0.005f, g.y + 0.048f, g.z + 0.65f},
        {0.058f, 0.058f, 0.035f}, BODY_DARK); // muzzle brake base
    // Muzzle brake slats.
    box({g.x - 0.005f, g.y + 0.065f, g.z + 0.655f},
        {0.062f, 0.010f, 0.025f}, BODY_DARK);
    box({g.x - 0.005f, g.y + 0.030f, g.z + 0.655f},
        {0.062f, 0.010f, 0.025f}, BODY_DARK);

    // Operator (sniper) gets a big scope instead of the iron rail.
    if (w.stats.name == "Operator") {
        box({g.x - 0.005f, g.y + 0.170f, g.z + 0.08f},
            {0.055f, 0.060f, 0.260f}, BODY_LIGHT);
        box({g.x - 0.005f, g.y + 0.170f, g.z + 0.22f},
            {0.070f, 0.080f, 0.050f}, BODY_DARK);
        box({g.x - 0.005f, g.y + 0.170f, g.z + 0.24f},
            {0.058f, 0.065f, 0.008f}, Color{80, 120, 180, 255}); // front lens
        box({g.x - 0.005f, g.y + 0.170f, g.z - 0.06f},
            {0.055f, 0.058f, 0.008f}, Color{40, 50, 70, 255}); // rear lens
    }
}

void drawPistol(Basis b, const WeaponAnim& anim) {
    const Vector3 g = b.grip;
    // Slide cycles backward during the bolt-pull phase.
    const float slideZ = -anim.boltBack * 0.08f;

    // Slide (top of pistol).
    box({g.x + 0.010f, g.y + 0.058f, g.z + 0.08f + slideZ},
        {0.055f, 0.068f, 0.240f}, BODY_LIGHT);
    // Slide serrations (rear).
    for (int i = 0; i < 4; ++i) {
        box({g.x + 0.040f, g.y + 0.058f, g.z + 0.01f + slideZ + i * 0.014f},
            {0.006f, 0.066f, 0.006f}, BODY_DARK);
    }
    // Front sight (moves with slide).
    box({g.x + 0.010f, g.y + 0.102f, g.z + 0.18f + slideZ},
        {0.014f, 0.020f, 0.014f}, BODY_DARK);
    // Rear sight (moves with slide).
    box({g.x + 0.010f, g.y + 0.100f, g.z - 0.02f + slideZ},
        {0.032f, 0.022f, 0.020f}, BODY_DARK);
    // Lower frame (under the slide) - stationary.
    box({g.x + 0.010f, g.y + 0.005f, g.z + 0.04f},
        {0.060f, 0.055f, 0.150f}, BODY_MID);
    // Trigger guard.
    box({g.x + 0.010f, g.y - 0.028f, g.z + 0.035f},
        {0.014f, 0.035f, 0.055f}, BODY_DARK);
    // Barrel nub visible at the front.
    box({g.x + 0.010f, g.y + 0.055f, g.z + 0.215f},
        {0.030f, 0.032f, 0.035f}, GUN_METAL);

    // During reload the mag visibly drops and a new one rises into the grip.
    if (anim.newMagY < 0.0f) {
        // Smaller pistol mag, visible below the grip as it slides in.
        float dy = anim.newMagY * 0.20f;
        box({g.x + 0.010f, g.y - 0.080f + dy, g.z + 0.00f},
            {0.050f, 0.125f, 0.065f}, MAG_COLOR);
        box({g.x + 0.010f, g.y - 0.155f + dy, g.z + 0.00f},
            {0.055f, 0.015f, 0.070f}, BODY_DARK);
    }
}

void drawKnife(Basis b) {
    const Vector3 g = b.grip;
    // Handle.
    box({g.x + 0.020f, g.y + 0.000f, g.z + 0.06f},
        {0.038f, 0.060f, 0.160f}, Color{28, 30, 38, 255});
    box({g.x + 0.020f, g.y + 0.000f, g.z + 0.04f},
        {0.044f, 0.066f, 0.018f}, BRASS);
    box({g.x + 0.020f, g.y + 0.000f, g.z + 0.10f},
        {0.044f, 0.066f, 0.018f}, BRASS);
    // Crossguard.
    box({g.x + 0.020f, g.y + 0.000f, g.z + 0.17f},
        {0.085f, 0.080f, 0.024f}, BRASS);
    // Blade.
    box({g.x + 0.020f, g.y + 0.015f, g.z + 0.33f},
        {0.022f, 0.042f, 0.290f}, BLADE);
    // Blade tip.
    box({g.x + 0.020f, g.y + 0.015f, g.z + 0.48f},
        {0.028f, 0.050f, 0.022f}, Color{240, 245, 250, 255});
}

void drawSmoke(Basis b) {
    const Vector3 g = b.grip;
    DrawSphere({g.x + 0.015f, g.y - 0.015f, g.z + 0.08f}, 0.075f, SMOKE_GRN);
    DrawSphere({g.x + 0.015f, g.y + 0.055f, g.z + 0.08f}, 0.036f,
               Color{60, 70, 55, 255});
    box({g.x + 0.045f, g.y + 0.065f, g.z + 0.08f},
        {0.024f, 0.034f, 0.018f}, BRASS);
}

// ----- Hands + arms ---------------------------------------------------------
// Left hand is the PROMINENT one, wrapped around the handguard. The right
// hand is mostly hidden by the receiver and only peeks out at the pistol grip.
void drawRightGripHand(Basis b) {
    const Vector3 g = b.grip;
    // Right glove / knuckles — mostly hidden behind the trigger housing.
    box({g.x + 0.045f, g.y - 0.060f, g.z - 0.02f},
        {0.075f, 0.130f, 0.115f}, GLOVE);
    // Thumb draping over the top of the grip.
    box({g.x - 0.002f, g.y + 0.015f, g.z - 0.02f},
        {0.040f, 0.040f, 0.075f}, SKIN);
    // A sliver of right forearm dropping off the bottom-right corner.
    Vector3 elbow{g.x + 0.160f, g.y - 0.56f, g.z - 0.24f};
    Vector3 wrist{g.x + 0.080f, g.y - 0.12f, g.z - 0.05f};
    DrawCylinderEx(elbow, wrist, 0.070f, 0.055f, 10, SKIN);
    // Sleeve cuff.
    DrawCylinderEx(elbow,
                   {elbow.x + 0.005f, elbow.y + 0.06f, elbow.z + 0.02f},
                   0.086f, 0.080f, 10, GLOVE_RIM);
}

void drawLeftForegripHand(Basis b, bool isRifle, float handDrop) {
    if (!isRifle) return;
    // During the mid-reload window, the hand slides down-and-out of frame to
    // fetch the new mag. `handDrop` is 0 at the grip and 1 fully off-screen.
    if (handDrop >= 0.98f) return;

    const Vector3 g = b.grip;
    const float dy = -0.55f * handDrop;
    const float dz = -0.10f * handDrop;
    const float alpha = 1.0f - 0.6f * clampf(handDrop, 0.0f, 1.0f);
    Color glove{GLOVE.r, GLOVE.g, GLOVE.b, static_cast<unsigned char>(255 * alpha)};
    Color gloveRim{GLOVE_RIM.r, GLOVE_RIM.g, GLOVE_RIM.b,
                   static_cast<unsigned char>(255 * alpha)};
    Color skin{SKIN.r, SKIN.g, SKIN.b, static_cast<unsigned char>(255 * alpha)};

    // Left glove wrapped around the handguard, prominent.
    box({g.x - 0.005f, g.y - 0.030f + dy, g.z + 0.34f + dz},
        {0.110f, 0.110f, 0.120f}, glove);
    // Finger wraps: three dark strips across the handguard.
    for (int i = 0; i < 3; ++i) {
        float z = g.z + 0.300f + i * 0.040f + dz;
        box({g.x - 0.005f, g.y - 0.085f + dy, z},
            {0.115f, 0.018f, 0.022f}, gloveRim);
    }
    // Knuckle rise on top of the hand.
    box({g.x - 0.005f, g.y + 0.030f + dy, g.z + 0.34f + dz},
        {0.100f, 0.020f, 0.090f}, gloveRim);
    // Left forearm: curves from below-left of the frame up to the hand.
    Vector3 elbow{g.x + 0.00f, g.y - 0.58f + dy, g.z + 0.10f + dz};
    Vector3 wrist{g.x + 0.00f, g.y - 0.10f + dy, g.z + 0.28f + dz};
    DrawCylinderEx(elbow, wrist, 0.080f, 0.060f, 12, skin);
    // Sleeve cuff around the forearm.
    DrawCylinderEx(elbow,
                   {elbow.x, elbow.y + 0.07f, elbow.z + 0.01f},
                   0.092f, 0.085f, 12, gloveRim);
}

// ----- Muzzle flash + brass casings ----------------------------------------
void drawMuzzleFlash(Basis b, float life, WeaponKind kind) {
    if (life <= 0.0f) return;
    // Barrel tip is at different Z depending on weapon.
    float tipZ;
    float tipY = b.grip.y + 0.048f;
    switch (kind) {
    case WeaponKind::Rifle:  tipZ = b.grip.z + 0.69f; break;
    case WeaponKind::Pistol: tipZ = b.grip.z + 0.24f; tipY = b.grip.y + 0.055f; break;
    default: return; // melee / smoke don't flash
    }
    Vector3 tip{b.grip.x - 0.005f, tipY, tipZ};

    // Bright white-hot core.
    float core = 0.055f * life;
    DrawSphere(tip, core, Color{255, 250, 210, static_cast<unsigned char>(240 * life)});
    // Outer orange halo.
    float halo = 0.10f + 0.05f * life;
    DrawSphere(tip, halo,
               Color{255, 170, 60, static_cast<unsigned char>(180 * life)});
    // Star spikes: four thin boxes radiating from the tip.
    Color spikeColor{255, 200, 90, static_cast<unsigned char>(210 * life)};
    box({tip.x,          tip.y,          tip.z + 0.09f * life},
        {0.040f * life,  0.040f * life,  0.14f * life}, spikeColor);
    box({tip.x,          tip.y + 0.06f * life, tip.z},
        {0.030f * life,  0.10f * life,   0.030f * life}, spikeColor);
    box({tip.x,          tip.y - 0.06f * life, tip.z},
        {0.030f * life,  0.10f * life,   0.030f * life}, spikeColor);
}

void drawCasings(const ViewModelState& s) {
    for (const BrassCasing& c : s.casings) {
        float fade = std::max(0.0f, 1.0f - c.life / 0.55f);
        unsigned char a = static_cast<unsigned char>(255 * fade);
        Color color{BRASS.r, BRASS.g, BRASS.b, a};
        // Draw as a tiny elongated box to suggest a cylinder.
        box(c.pos, {0.012f, 0.012f, 0.028f}, color);
    }
}

} // namespace

// ----- Simulation ----------------------------------------------------------
void tickViewModel(ViewModelState& s, float dt, float horizontalSpeed,
                   bool firedThisTick, int weaponSlot) {
    if (firedThisTick) {
        s.kickLife = 1.0f;
        s.flashLife = 1.0f;
        // Spawn an ejecting brass casing from the ejection port (screen-right
        // side of the receiver). Ejected toward world +X (screen-right) with
        // a bit of upward velocity.
        BrassCasing c{};
        c.pos = Vector3{-0.18f, -0.08f, 0.73f};
        c.vel = Vector3{
             0.80f + randf() * 0.30f,
             0.40f + randf() * 0.20f,
            -0.10f + (randf() - 0.5f) * 0.20f,
        };
        c.rotSpeed = (randf() * 12.0f - 6.0f);
        s.casings.push_back(c);
    }
    s.kickLife  = std::max(0.0f, s.kickLife  - dt * 8.0f);
    s.flashLife = std::max(0.0f, s.flashLife - dt * 30.0f); // very fast decay

    // Update casings.
    for (BrassCasing& c : s.casings) {
        c.vel.y  -= 3.0f * dt; // gravity
        c.pos.x += c.vel.x * dt;
        c.pos.y += c.vel.y * dt;
        c.pos.z += c.vel.z * dt;
        c.rotation += c.rotSpeed * dt;
        c.life += dt;
    }
    s.casings.erase(
        std::remove_if(s.casings.begin(), s.casings.end(),
                       [](const BrassCasing& c) { return c.life > 0.55f; }),
        s.casings.end());

    // Walk bob.
    float targetBob = clampf(horizontalSpeed / 6.0f, 0.0f, 1.0f);
    s.bobAmount += (targetBob - s.bobAmount) * std::min(1.0f, dt * 8.0f);
    s.bobPhase += (0.8f + 1.2f * s.bobAmount) * dt * 9.0f;

    // Weapon swap ease-in.
    if (weaponSlot != s.lastWeaponSlot) {
        s.swapBlend = 0.0f;
        s.lastWeaponSlot = weaponSlot;
    }
    s.swapBlend = std::min(1.0f, s.swapBlend + dt * 5.0f);
}

// ----- Rendering -----------------------------------------------------------
void drawViewModel(const ViewModelState& s, const Weapon& weapon) {
    Camera3D cam{};
    cam.position   = Vector3{0, 0, 0};
    cam.target     = Vector3{0, 0, 1};
    cam.up         = Vector3{0, 1, 0};
    cam.fovy       = 55.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Reload progress (0..1) drives all the reload-phase parameters.
    const Vector3 gripIdle{-0.24f, -0.22f, 0.55f};
    WeaponAnim anim{};
    if (weapon.reloadLeft > 0.0f && weapon.stats.reloadTime > 0.0f) {
        float t = 1.0f - (weapon.reloadLeft / weapon.stats.reloadTime);
        anim = computeReloadAnim(t, weapon.stats.kind, gripIdle);
    }

    // Bob, kick, swap, and reload offsets combine into the grip anchor.
    const float bob   = s.bobAmount;
    const float bobY  = std::sin(s.bobPhase) * 0.010f * bob;
    const float bobX  = std::cos(s.bobPhase * 0.5f) * 0.008f * bob;
    const float kickY =  s.kickLife * 0.032f;
    const float kickZ = -s.kickLife * 0.100f;
    const float swapY = -(1.0f - s.swapBlend) * 0.35f;
    const float reloadY = -anim.dip * 0.22f;
    const float reloadZ = -anim.dip * 0.04f; // pull slightly toward the player

    Basis b{};
    b.grip = Vector3{
        gripIdle.x + bobX,
        gripIdle.y + bobY + kickY + swapY + reloadY + anim.jostleY,
        gripIdle.z + kickZ + reloadZ,
    };

    BeginMode3D(cam);
    rlDrawRenderBatchActive();
    rlDisableDepthTest();

    // Roll the viewmodel around the view axis (barrel-up-left tilt),
    // with extra inward tilt while reloading.
    rlPushMatrix();
    rlTranslatef(b.grip.x, b.grip.y, b.grip.z);
    rlRotatef(ROLL_DEGREES + anim.tiltExtra, 0.0f, 0.0f, 1.0f);
    rlTranslatef(-b.grip.x, -b.grip.y, -b.grip.z);

    // Draw order (painter's algorithm, depth test off):
    //   1) Right forearm + glove (behind the gun)
    //   2) Weapon body + stock + magazine (w/ reload-aware parts)
    //   3) Left arm + foregrip hand (in front of the handguard)
    //   4) Muzzle + brass (in front of everything)
    drawRightGripHand(b);

    bool isRifle = weapon.stats.kind == WeaponKind::Rifle;
    switch (weapon.stats.kind) {
    case WeaponKind::Rifle:  drawRifle(b, weapon, anim); break;
    case WeaponKind::Pistol: drawPistol(b, anim); break;
    case WeaponKind::Knife:  drawKnife(b); break;
    case WeaponKind::Smoke:  drawSmoke(b); break;
    }

    drawLeftForegripHand(b, isRifle, anim.leftHandDrop);

    rlPopMatrix();

    // Detached falling magazine: drawn outside the roll so it falls along
    // world-space axes regardless of gun tilt.
    if (anim.showDetachedMag) {
        Color mag  {MAG_COLOR.r, MAG_COLOR.g, MAG_COLOR.b,
                    static_cast<unsigned char>(255 * anim.detachedMagAlpha)};
        Color pad  {BODY_DARK.r, BODY_DARK.g, BODY_DARK.b,
                    static_cast<unsigned char>(255 * anim.detachedMagAlpha)};
        const Vector3 p = anim.detachedMagPos;
        if (weapon.stats.kind == WeaponKind::Rifle) {
            box(p, {0.080f, 0.175f, 0.120f}, mag);
            box({p.x, p.y - 0.095f, p.z}, {0.088f, 0.020f, 0.128f}, pad);
        } else { // pistol
            box(p, {0.050f, 0.125f, 0.065f}, mag);
            box({p.x, p.y - 0.070f, p.z}, {0.055f, 0.015f, 0.070f}, pad);
        }
    }

    // Muzzle flash + brass casings (in world-axis coords so they don't tilt).
    drawMuzzleFlash(b, s.flashLife, weapon.stats.kind);
    drawCasings(s);

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    EndMode3D();
}

} // namespace fps
