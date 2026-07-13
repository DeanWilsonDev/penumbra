#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Render/Color.h"
#include "Penumbra/Render/IFontBackend.h"

#include <SDL3/SDL.h>

#include <vector>

namespace Penumbra::Render {

// SDL_BLENDMODE_BLEND (the default, "over" compositing) or SDL_BLENDMODE_ADD
// (additive -- overlapping translucent draws brighten instead of just
// compositing more opaque). See Renderer::PushBlendMode.
enum class BlendMode { Normal, Additive };

// One color stop in a multi-stop DrawRadialGradient: T is the stop's position as a
// fraction of RadiusLogical (0 = centre, 1 = the disc's outer edge); StopColor is the
// solid color at that stop. Stops must be supplied in ascending T order; the first
// stop's own T is ignored -- it is always the disc's exact centre point.
struct GradientStop {
    float T{0.0f};
    Color StopColor;
};

// Wraps SDL_Renderer and exposes a logical-pixel drawing API. The renderer
// applies the DPI scale to geometry at submission; glyph textures are drawn at
// their native physical size (no double scaling). This is the only layer an
// SDL_GPU port would replace.
class Renderer {
public:
    bool Initialise(SDL_Renderer* SdlRenderer, float DpiScaleFactor, IFontBackend* FontBackend);

    void BeginFrame(Color ClearColor);
    void EndFrameAndPresent();

    // All coordinates are LOGICAL pixels; the renderer multiplies by DPI scale internally.
    // CornerRadiusLogical 0 (the default) draws square corners via the fast path;
    // a positive radius tessellates a rounded rect through SDL_RenderGeometry.
    void DrawFilledRect (Rect RectLogical, Color InColor, float CornerRadiusLogical = 0.0f);
    void DrawRectOutline(Rect RectLogical, Color InColor, float ThicknessLogical,
                         float CornerRadiusLogical = 0.0f);
    void DrawText       (FontHandle Font, std::string_view Text, Point PositionLogical, Color InColor);

    // A 2-stop vertical gradient (TopColor at the rect's top edge, BottomColor at
    // its bottom edge), otherwise identical to DrawFilledRect -- same rounded-
    // corner handling, same anti-aliased edge. CornerRadiusLogical 0 draws a
    // gradient-filled quad via SDL_RenderGeometry (4 vertices, one flat quad)
    // instead of the SDL_RenderFillRect fast path DrawFilledRect uses, since that
    // fast path has no per-vertex color; a positive radius reuses the existing
    // rounded-rect tessellation with each vertex's color lerped by its
    // normalised Y position.
    void DrawGradientRect(Rect RectLogical, Color TopColor, Color BottomColor,
                          float CornerRadiusLogical = 0.0f);

    // A radial gradient centred at Centre, solid CentreColor at the middle,
    // interpolating outward to EdgeColor at RadiusLogical. Same solid-centre-fan
    // vertex-color technique DrawFilledRect and DrawGradientRect's rounded-corner
    // path already use -- a centre vertex plus a fading outer ring, here keyed by
    // radial distance instead of Y position -- and independent of any caster rect,
    // so it draws a filled disc directly instead of borrowing DrawDropShadow's
    // ring-around-a-shape contract.
    void DrawRadialGradient(Point Centre, float RadiusLogical, Color CentreColor, Color EdgeColor);

    // Multi-stop form of the above: an arbitrary number of color stops instead of a
    // fixed centre/edge pair (e.g. a bright white core -> a tinted accent midtone ->
    // a transparent edge, in one draw call). The 2-color overload is a thin wrapper
    // around this one ({T=0, CentreColor}, {T=1, EdgeColor}); the AA feather is always
    // applied past the last stop, the same way the 2-color overload fades past EdgeColor.
    void DrawRadialGradient(Point Centre, float RadiusLogical, const std::vector<GradientStop>& Stops);

    // A soft rectangular shadow, meant to be drawn just before the widget that
    // casts it so it reads as sitting "behind" it. RectLogical is the caster's
    // own rect; the shadow extends BlurRadiusLogical beyond it on every side,
    // solid-alpha (per ShadowColor) at the caster's edge, fading to fully
    // transparent at the outer edge. Reuses the rounded-rect ring tessellation
    // (BuildRoundedRing) already built for anti-aliasing, just with the fringe
    // width set to BlurRadiusLogical instead of the fixed 1px AA feather.
    void DrawDropShadow(Rect RectLogical, Color ShadowColor, float BlurRadiusLogical,
                        float CornerRadiusLogical = 0.0f);

    // A straight line segment, ThicknessLogical wide, with the same anti-aliased
    // edge treatment as DrawRectOutline.
    void DrawLine(Point From, Point To, Color InColor, float ThicknessLogical);

    // A solid-filled triangle -- enough, combined with DrawLine, to build a
    // chevron (two lines) or a filled caret/arrow (one triangle) as an app-level
    // helper, the same way Pharos already builds its own composite widgets on
    // top of Box.
    void DrawTriangleFilled(Point A, Point B, Point C, Color InColor);

    // Returns the underlying SDL_Renderer so ViewportWidget can manage its off-screen
    // scene texture (create, switch target). Penumbra internals only — Dawn never calls
    // this; it keeps SDL render-target juggling locked inside the Render layer.
    SDL_Renderer* GetSdlRenderer() const;

    // Draws a previously acquired SDL_Texture into a logical-pixel destination rect.
    // Used by ViewportWidget to composite the scene texture back into the UI pass.
    void DrawTexture(SDL_Texture* Texture, Rect DestLogical);

    // Clip stack — every push must be matched by a pop. Nested pushes intersect.
    // Asserted balanced at EndFrameAndPresent.
    void PushClipRect(Rect RectLogical);
    void PopClipRect();

    // Blend-mode stack, scoped like the clip-rect stack above -- every push must be
    // matched by a pop. Nesting is not expected (SDL's blend mode is a single current
    // state, not a stack of composited layers), but a stack keeps the call symmetric
    // with PushClipRect/PopClipRect and makes "did I forget to restore this"
    // debug-assertable the same way unbalanced clip pushes already are. Asserted
    // balanced at EndFrameAndPresent.
    void PushBlendMode(BlendMode Mode);
    void PopBlendMode();

    // The fully general escape hatch PushBlendMode(BlendMode::Additive) is a convenience
    // wrapper around: composes an arbitrary blend mode from SDL's six blend-factor/
    // operation parameters (SDL_ComposeCustomBlendMode) for callers that need something
    // neither Normal nor Additive covers (e.g. a multiply or screen blend). Pushes onto
    // the same stack as PushBlendMode -- pop either kind of push with PopBlendMode.
    void PushCustomBlendMode(SDL_BlendFactor SrcColorFactor, SDL_BlendFactor DstColorFactor,
                             SDL_BlendOperation ColorOperation, SDL_BlendFactor SrcAlphaFactor,
                             SDL_BlendFactor DstAlphaFactor, SDL_BlendOperation AlphaOperation);

    // Measurement is in logical pixels even though glyphs rasterise at physical size.
    TextMetrics MeasureText     (FontHandle Font, std::string_view Text) const;
    float       MeasureTextWidth(FontHandle Font, std::string_view Text) const; // caret positioning

    float GetDpiScaleFactor() const;

    // Updates the scale factor used to convert logical -> physical pixels for
    // every subsequent Draw* call. Call once per frame with the platform's
    // current value (e.g. from PlatformWindow::GetDpiScaleFactor()) -- cheap to
    // call even when the value hasn't changed.
    void SetDpiScaleFactor(float NewScaleFactor);

private:
    SDL_FRect ToPhysical(Rect RectLogical) const;

    SDL_Renderer*         SdlRenderer{nullptr};
    IFontBackend*         FontBackend{nullptr};
    float                 DpiScaleFactor{1.0f};
    std::vector<SDL_Rect>      ClipStack;  // physical-pixel rects, already intersected
    std::vector<SDL_BlendMode> BlendStack; // resolved SDL blend modes -- PushBlendMode and
                                            // PushCustomBlendMode both push onto this one stack
};

} // namespace Penumbra::Render
