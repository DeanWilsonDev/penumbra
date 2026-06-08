#include "Penumbra/Render/Renderer.h"

namespace Penumbra::Render {

bool Renderer::Initialise(SDL_Renderer* InSdlRenderer, float InDpiScaleFactor,
                          IFontBackend* InFontBackend) {
    if (!InSdlRenderer) {
        return false;
    }
    SdlRenderer = InSdlRenderer;
    DpiScaleFactor = (InDpiScaleFactor > 0.0f) ? InDpiScaleFactor : 1.0f;
    FontBackend = InFontBackend; // unused until Milestone 1
    return true;
}

void Renderer::BeginFrame(SDL_Color ClearColor) {
    SDL_SetRenderDrawColor(SdlRenderer, ClearColor.r, ClearColor.g, ClearColor.b, ClearColor.a);
    SDL_RenderClear(SdlRenderer);
}

void Renderer::EndFrameAndPresent() {
    SDL_RenderPresent(SdlRenderer);
}

float Renderer::GetDpiScaleFactor() const {
    return DpiScaleFactor;
}

} // namespace Penumbra::Render
