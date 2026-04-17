#pragma once

#include <raylib.h>

namespace fps {

class SoundBank {
public:
    Sound rifle{};
    Sound pistol{};
    Sound knife{};
    Sound reload{};
    Sound hitmarker{};
    Sound footstep{};
    Sound smokeThrow{};
    Sound smokePuff{};

    void load();
    void unload();

private:
    Wave waves_[8]{};
    bool loaded_ = false;
};

} // namespace fps
