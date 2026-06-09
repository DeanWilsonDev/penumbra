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
    // CornerRadiusLogical 0 (the default) draws square corners via the fast path;
    // a positive radius tessellates a rounded rect through SDL_RenderGeometry.
    void DrawFilledRect (SDL_FRect RectLogical, SDL_Color Color, float CornerRadiusLogical = 0.0f);
    void DrawRectOutline(SDL_FRect RectLogical, SDL_Color Color, float ThicknessLogical,
                         float CornerRadiusLogical = 0.0f);
    void DrawText       (FontHandle Font, std::string_view Text, SDL_FPoint PositionLogical, SDL_Color Color);

    // Returns the underlying SDL_Renderer so ViewportWidget can manage its off-screen
    // scene texture (create, switch target). Penumbra internals only — Dawn never calls
    // this; it keeps SDL render-target juggling locked inside the Render layer.
    SDL_Renderer* GetSdlRenderer() const;

    // Draws a previously acquired SDL_Texture into a logical-pixel destination rect.
    // Used by ViewportWidget to composite the scene texture back into the UI pass.
    void DrawTexture(SDL_Texture* Texture, SDL_FRect DestLogical);

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
