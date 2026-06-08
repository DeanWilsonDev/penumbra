#pragma once

#include <SDL3/SDL.h>

namespace Penumbra::Render {

class IFontBackend; // defined in Milestone 1

// Wraps SDL_Renderer and exposes a logical-pixel drawing API. The renderer
// applies the DPI scale at submission. This is the only layer an SDL_GPU port
// would replace.
//
// Milestone 0 subset: bring-up only (clear + present). Primitive drawing, the
// clip stack, text, and measurement arrive in Milestone 1.
class Renderer {
public:
    bool Initialise(SDL_Renderer* SdlRenderer, float DpiScaleFactor, IFontBackend* FontBackend);

    void BeginFrame(SDL_Color ClearColor);
    void EndFrameAndPresent();

    float GetDpiScaleFactor() const;

private:
    SDL_Renderer* SdlRenderer{nullptr};
    IFontBackend* FontBackend{nullptr};
    float         DpiScaleFactor{1.0f};
};

} // namespace Penumbra::Render
