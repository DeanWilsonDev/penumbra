#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace Penumbra::Platform {

// A snapshot of all OS input for a single frame, built by PlatformWindow.
// This is the one struct above-platform layers are allowed to see SDL types
// through, because input is inherently expressed in SDL's vocabulary.
struct InputState {
    SDL_FPoint MousePosition{0.0f, 0.0f};            // logical pixels
    bool       MouseButtonDown[3]{};                 // left, middle, right — current
    bool       MouseButtonPressedThisFrame[3]{};
    bool       MouseButtonReleasedThisFrame[3]{};
    float      MouseWheelDelta{0.0f};                // accumulated this frame

    std::string              TextInputThisFrame;       // from SDL text-input events
    std::vector<SDL_Keycode> KeysPressedThisFrame;     // arrows, backspace, delete, etc.
    SDL_Keymod               ModifierState{SDL_KMOD_NONE};
};

} // namespace Penumbra::Platform
