#pragma once

#include <SDL3/SDL.h>

namespace Demo {

// The app owns the vocabulary AND the values. Penumbra never sees these names.
// This is the ONLY place in the project where values and aesthetics live.
struct Theme {
    float SpacingSmall  = 8.0f;
    float SpacingMedium = 12.0f;
    float SpacingLarge  = 16.0f;

    SDL_Color ColorBackgroundPrimary = {26, 26, 26, 255};
    SDL_Color ColorSurfaceRaised     = {58, 58, 65, 255};
    SDL_Color ColorAccent            = {77, 127, 235, 255};
    SDL_Color ColorTextPrimary       = {235, 235, 235, 255};
    SDL_Color ColorBorderDefault     = {70, 70, 75, 255};

    float BorderRadiusSmall  = 3.0f;
    float BorderWidthDefault = 1.0f;
};

} // namespace Demo
