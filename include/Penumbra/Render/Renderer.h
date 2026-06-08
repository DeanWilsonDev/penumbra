#pragma once

#include "Penumbra/Render/IFontBackend.h"

#include <SDL3/SDL.h>

#include <vector>

namespace Penumbra::Render {

// Wraps SDL_Renderer and exposes a logical-pixel drawing API. The renderer
// applies the DPI scale to geometry at submission; glyph textures are drawn at
// their native physical size (no double scaling). This is the only layer an
// SDL_GPU port would replace.
class Renderer {
public:
    bool Initialise(SDL_Renderer* SdlRenderer, float DpiScaleFactor, IFontBackend* FontBackend);

    void BeginFrame(SDL_Color ClearColor);
    void EndFrameAndPresent();

    // All coordinates are LOGICAL pixels; the renderer multiplies by DPI scale internally.
    void DrawFilledRect (SDL_FRect RectLogical, SDL_Color Color);
    void DrawRectOutline(SDL_FRect RectLogical, SDL_Color Color, float ThicknessLogical);
    void DrawText       (FontHandle Font, std::string_view Text, SDL_FPoint PositionLogical, SDL_Color Color);

    // Clip stack — every push must be matched by a pop. Nested pushes intersect.
    // Asserted balanced at EndFrameAndPresent.
    void PushClipRect(SDL_FRect RectLogical);
    void PopClipRect();

    // Measurement is in logical pixels even though glyphs rasterise at physical size.
    TextMetrics MeasureText     (FontHandle Font, std::string_view Text) const;
    float       MeasureTextWidth(FontHandle Font, std::string_view Text) const; // caret positioning

    float GetDpiScaleFactor() const;

private:
    SDL_FRect ToPhysical(SDL_FRect RectLogical) const;

    SDL_Renderer*         SdlRenderer{nullptr};
    IFontBackend*         FontBackend{nullptr};
    float                 DpiScaleFactor{1.0f};
    std::vector<SDL_Rect> ClipStack; // physical-pixel rects, already intersected
};

} // namespace Penumbra::Render
