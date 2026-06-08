#include "Penumbra/Render/Renderer.h"

#include <cassert>
#include <cmath>

namespace Penumbra::Render {

bool Renderer::Initialise(SDL_Renderer* InSdlRenderer, float InDpiScaleFactor,
                          IFontBackend* InFontBackend) {
    if (!InSdlRenderer) {
        return false;
    }
    SdlRenderer = InSdlRenderer;
    DpiScaleFactor = (InDpiScaleFactor > 0.0f) ? InDpiScaleFactor : 1.0f;
    FontBackend = InFontBackend;
    return true;
}

SDL_FRect Renderer::ToPhysical(SDL_FRect RectLogical) const {
    return {RectLogical.x * DpiScaleFactor, RectLogical.y * DpiScaleFactor,
            RectLogical.w * DpiScaleFactor, RectLogical.h * DpiScaleFactor};
}

void Renderer::BeginFrame(SDL_Color ClearColor) {
    SDL_SetRenderDrawColor(SdlRenderer, ClearColor.r, ClearColor.g, ClearColor.b, ClearColor.a);
    SDL_RenderClear(SdlRenderer);
}

void Renderer::EndFrameAndPresent() {
    assert(ClipStack.empty() && "clip stack not balanced: a PushClipRect is missing its PopClipRect");
    SDL_RenderPresent(SdlRenderer);
}

void Renderer::DrawFilledRect(SDL_FRect RectLogical, SDL_Color Color) {
    SDL_SetRenderDrawColor(SdlRenderer, Color.r, Color.g, Color.b, Color.a);
    const SDL_FRect Physical = ToPhysical(RectLogical);
    SDL_RenderFillRect(SdlRenderer, &Physical);
}

void Renderer::DrawRectOutline(SDL_FRect RectLogical, SDL_Color Color, float ThicknessLogical) {
    SDL_SetRenderDrawColor(SdlRenderer, Color.r, Color.g, Color.b, Color.a);
    const SDL_FRect R = ToPhysical(RectLogical);
    const float T = ThicknessLogical * DpiScaleFactor;

    const SDL_FRect Edges[4] = {
        {R.x,           R.y,           R.w, T  }, // top
        {R.x,           R.y + R.h - T, R.w, T  }, // bottom
        {R.x,           R.y,           T,   R.h}, // left
        {R.x + R.w - T, R.y,           T,   R.h}, // right
    };
    for (const SDL_FRect& Edge : Edges) {
        SDL_RenderFillRect(SdlRenderer, &Edge);
    }
}

void Renderer::DrawText(FontHandle Font, std::string_view Text, SDL_FPoint PositionLogical,
                        SDL_Color Color) {
    if (!FontBackend || Text.empty()) {
        return;
    }

    SDL_Texture* Texture = FontBackend->AcquireTextTexture(SdlRenderer, Font, Text, Color);
    if (!Texture) {
        return;
    }

    // Glyph texture is already at physical pixel size; only the position scales.
    float TextureWidth = 0.0f;
    float TextureHeight = 0.0f;
    SDL_GetTextureSize(Texture, &TextureWidth, &TextureHeight);

    const SDL_FRect Destination{PositionLogical.x * DpiScaleFactor,
                                PositionLogical.y * DpiScaleFactor, TextureWidth, TextureHeight};
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}

void Renderer::PushClipRect(SDL_FRect RectLogical) {
    const SDL_FRect P = ToPhysical(RectLogical);
    SDL_Rect Requested{static_cast<int>(std::lround(P.x)), static_cast<int>(std::lround(P.y)),
                       static_cast<int>(std::lround(P.w)), static_cast<int>(std::lround(P.h))};

    SDL_Rect Effective = Requested;
    if (!ClipStack.empty()) {
        // Intersect with the active clip so nested clips never exceed their parent.
        if (!SDL_GetRectIntersection(&ClipStack.back(), &Requested, &Effective)) {
            Effective = {0, 0, 0, 0}; // disjoint → clip everything
        }
    }

    ClipStack.push_back(Effective);
    SDL_SetRenderClipRect(SdlRenderer, &Effective);
}

void Renderer::PopClipRect() {
    assert(!ClipStack.empty() && "PopClipRect called with an empty clip stack");
    ClipStack.pop_back();
    if (ClipStack.empty()) {
        SDL_SetRenderClipRect(SdlRenderer, nullptr);
    } else {
        SDL_SetRenderClipRect(SdlRenderer, &ClipStack.back());
    }
}

TextMetrics Renderer::MeasureText(FontHandle Font, std::string_view Text) const {
    if (!FontBackend) {
        return {0.0f, 0.0f, 0.0f};
    }
    return FontBackend->MeasureText(Font, Text);
}

float Renderer::MeasureTextWidth(FontHandle Font, std::string_view Text) const {
    if (!FontBackend) {
        return 0.0f;
    }
    return FontBackend->MeasureTextWidth(Font, Text);
}

float Renderer::GetDpiScaleFactor() const {
    return DpiScaleFactor;
}

} // namespace Penumbra::Render
