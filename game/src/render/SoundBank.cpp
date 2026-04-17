#include "SoundBank.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace fps {

namespace {

constexpr int SAMPLE_RATE = 22050;

template <typename Gen>
Wave makeWave(float seconds, Gen gen) {
    Wave w{};
    w.sampleRate = SAMPLE_RATE;
    w.sampleSize = 16;
    w.channels   = 1;
    w.frameCount = static_cast<unsigned int>(seconds * SAMPLE_RATE);
    int16_t* data = static_cast<int16_t*>(std::malloc(sizeof(int16_t) * w.frameCount));
    for (unsigned int i = 0; i < w.frameCount; ++i) {
        float s = gen(i, w.frameCount);
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        data[i] = static_cast<int16_t>(s * 30000.0f);
    }
    w.data = data;
    return w;
}

float rand01() { return static_cast<float>(std::rand()) * (1.0f / static_cast<float>(RAND_MAX)); }

Wave makeGunshot(float seconds, float bassFreq, float brightness) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = std::exp(-t * 28.0f);
        float noise = (rand01() * 2.0f - 1.0f) * brightness;
        float thump = std::sin(2.0f * PI * bassFreq * t) * 0.6f * std::exp(-t * 12.0f);
        (void)total;
        return (noise + thump) * env;
    });
}

Wave makeBeep(float seconds, float startHz, float endHz) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float f = startHz + (endHz - startHz) * t / seconds;
        float env = std::exp(-t * 12.0f);
        (void)total;
        return std::sin(2.0f * PI * f * t) * env * 0.6f;
    });
}

Wave makeHiss(float seconds) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = 1.0f - t / seconds;
        float noise = (rand01() * 2.0f - 1.0f);
        static float y = 0.0f;
        y = y * 0.85f + noise * 0.15f;
        (void)total;
        return y * env * 0.5f;
    });
}

Wave makeSwoosh(float seconds) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = std::exp(-t * 6.0f);
        float f = 800.0f - 600.0f * (t / seconds);
        (void)total;
        return std::sin(2.0f * PI * f * t) * env * 0.4f;
    });
}

Wave makeClick(float seconds) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = std::exp(-t * 50.0f);
        (void)total;
        return (rand01() * 2.0f - 1.0f) * env * 0.6f;
    });
}

Wave makeFootstep(float seconds) {
    return makeWave(seconds, [=](unsigned int i, unsigned int total) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = std::exp(-t * 35.0f);
        float noise = (rand01() * 2.0f - 1.0f);
        (void)total;
        return noise * env * 0.35f;
    });
}

} // namespace

void SoundBank::load() {
    if (loaded_) return;
    waves_[0] = makeGunshot(0.28f, 80.0f, 0.9f);
    waves_[1] = makeGunshot(0.22f, 120.0f, 0.7f);
    waves_[2] = makeSwoosh(0.30f);
    waves_[3] = makeClick(0.12f);
    waves_[4] = makeBeep(0.10f, 900.0f, 1300.0f);
    waves_[5] = makeFootstep(0.12f);
    waves_[6] = makeSwoosh(0.45f);
    waves_[7] = makeHiss(1.2f);

    rifle      = LoadSoundFromWave(waves_[0]);
    pistol     = LoadSoundFromWave(waves_[1]);
    knife      = LoadSoundFromWave(waves_[2]);
    reload     = LoadSoundFromWave(waves_[3]);
    hitmarker  = LoadSoundFromWave(waves_[4]);
    footstep   = LoadSoundFromWave(waves_[5]);
    smokeThrow = LoadSoundFromWave(waves_[6]);
    smokePuff  = LoadSoundFromWave(waves_[7]);
    loaded_ = true;
}

void SoundBank::unload() {
    if (!loaded_) return;
    UnloadSound(rifle);
    UnloadSound(pistol);
    UnloadSound(knife);
    UnloadSound(reload);
    UnloadSound(hitmarker);
    UnloadSound(footstep);
    UnloadSound(smokeThrow);
    UnloadSound(smokePuff);
    for (Wave& w : waves_) UnloadWave(w);
    loaded_ = false;
}

} // namespace fps
