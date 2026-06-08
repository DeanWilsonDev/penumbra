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
    SDL_Color ColorAccentHovered     = {97, 147, 255, 255};
    SDL_Color ColorAccentPressed     = {57, 107, 215, 255};
    SDL_Color ColorTextPrimary       = {235, 235, 235, 255};
    SDL_Color ColorTextDisabled      = {120, 120, 128, 255};
    SDL_Color ColorBorderDefault     = {70, 70, 75, 255};
    SDL_Color ColorControlDisabled   = {45, 45, 50, 255};

    float BorderRadiusSmall  = 3.0f;
    float BorderWidthDefault = 1.0f;

    float FontSizeBody = 16.0f;

    float CheckboxGlyphSize = 18.0f;  // edge length of the checkbox square
    float DragSensitivity   = 0.05f;  // value change per logical px dragged
    float FieldWidthSmall   = 160.0f; // min width for numeric/text fields
};

} // namespace Demo
