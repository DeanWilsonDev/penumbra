#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Render/Color.h"
#include "Penumbra/Render/IFontBackend.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <span>
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

// How DrawPolyline turns a corner and ends an open line, as SVG's stroke-linejoin
// and stroke-linecap do. A Miter join longer than four times the line's thickness
// (SVG's default miter limit) is drawn as a Bevel instead.
enum class LineJoin { Miter, Bevel, Round };
enum class LineCap { Butt, Square, Round };

// Wraps SDL_Renderer and exposes a logical-pixel drawing API. The renderer
// applies the DPI scale to geometry at submission; glyph textures are drawn at
// their native physical size (no double scaling). This is the only layer an
// SDL_GPU port would replace.
class Renderer {
public:
    bool Initialise(SDL_Renderer* SdlRenderer, float DpiScaleFactor, IFontBackend* FontBackend);

    // BeginFrame also advances the frame clock below. The one-argument form times the
    // frame itself; a host with its own frame time (Application passes the input's
    // DeltaTimeSeconds) hands it over instead, so both clocks agree.
    void BeginFrame(Color ClearColor);
    void BeginFrame(Color ClearColor, float FrameDeltaSeconds);
    void EndFrameAndPresent();

    // The frame clock, for widgets that animate on their own (charts, Box
    // transitions). They advance in Draw, which reaches every visible widget each
    // frame, where UpdateInteractionState stops at the first widget that consumes
    // the input. The index is 0 until the first BeginFrame and goes up by one per
    // frame, so a widget that remembers the index it last drew at can tell whether it
    // was hidden in between. The delta is clamped to 0.1s, as the platform clamps the
    // input's.
    float         GetFrameDeltaSeconds() const { return FrameDeltaSeconds; }
    std::uint64_t GetFrameIndex() const { return FrameIndex; }

    // All coordinates are LOGICAL pixels; the renderer multiplies by DPI scale internally.
    // CornerRadiusLogical 0 (the default) draws square corners via the fast path;
    // a positive radius tessellates a rounded rect through SDL_RenderGeometry.
    // A rect with no width or height draws nothing here, nor in DrawGradientRect.
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

    // A filled disc, with the same anti-aliased edge as DrawFilledRect.
    void DrawFilledCircle(Point Centre, float RadiusLogical, Color InColor);

    // A circle's outline, ThicknessLogical wide and drawn inside RadiusLogical, the
    // same way DrawRectOutline keeps its border inside the rect. A thickness of
    // RadiusLogical or more draws a filled disc.
    void DrawCircleOutline(Point Centre, float RadiusLogical, Color InColor, float ThicknessLogical);

    // The part of a ring between InnerRadiusLogical and OuterRadiusLogical that starts
    // at StartDegrees and sweeps SweepDegrees: a donut segment, or a progress ring.
    // Angles run clockwise from 3 o'clock, the same direction as
    // Transform::RotationDegrees, so -90 is 12 o'clock; a negative sweep runs
    // anticlockwise. InnerRadiusLogical 0 draws a pie slice, and a sweep of 360 or
    // more draws the whole ring. Every edge is anti-aliased, the two straight ends
    // included, so segments that share an end show a faint seam where both fringes
    // fade; a donut chart that wants none leaves a small gap between its segments.
    void DrawArc(Point Centre, float InnerRadiusLogical, float OuterRadiusLogical, float StartDegrees,
                 float SweepDegrees, Color InColor);

    // Line segments through Points, ThicknessLogical wide with DrawLine's anti-aliased
    // edge, built as one mesh so a translucent line doesn't darken at its joints. The
    // exception is a turn so sharp that its inside reaches back past a whole segment,
    // where the line overlaps itself. Repeated points are skipped. Closed also joins
    // the last point back to the first, and then Cap is unused.
    void DrawPolyline(std::span<const Point> Points, Color InColor, float ThicknessLogical,
                      LineJoin Join = LineJoin::Miter, LineCap Cap = LineCap::Butt, bool Closed = false);

    // A filled polygon with an anti-aliased edge. Points can wind either way and the
    // polygon can be concave, but its edges mustn't cross. It's triangulated by ear
    // clipping on every call, which is quick for the tens to low hundreds of points a
    // chart's area has. Like DrawArc, two polygons that share an edge show a faint
    // seam along it.
    void DrawFilledPolygon(std::span<const Point> Points, Color InColor);

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

    // Redirects subsequent drawing to an offscreen texture sized to SubjectRectLogical,
    // so a whole widget subtree can be painted once and then composited back as a
    // single scaled/rotated/translated blit (BoxStyle::Transform). A no-op push/pop
    // (drawing continues straight to the current target) when InTransform.IsIdentity()
    // and Opacity is 1 -- the common case costs nothing beyond the check. Nests: an inner push/pop composites into
    // whichever target -- the window, or an ancestor's own capture -- was active when
    // it was pushed, so nested transforms compose the way nested clip rects already
    // do. Every push must be matched by a pop; asserted balanced at
    // EndFrameAndPresent like the clip/blend stacks.
    //
    // Opacity below 1 captures too, even with an identity transform, and the capture
    // composites back that translucent (BoxStyle::Opacity), so overlapping children
    // fade as one layer rather than showing through each other.
    void PushTransform(Rect SubjectRectLogical, const Transform& InTransform, float Opacity = 1.0f);
    void PopTransform();

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
    Point     ToPhysicalPoint(Point PointLogical) const;

    // Points in physical pixels, without consecutive repeats; when IsClosed, also
    // without a last point that repeats the first.
    std::vector<SDL_FPoint> ToPhysicalPath(std::span<const Point> PointsLogical, bool IsClosed) const;

    // One PushTransform frame. CaptureTexture is null for an identity push (nothing
    // was redirected, Pop is a no-op) so the common untransformed path never
    // allocates a texture.
    struct TransformFrame {
        SDL_Texture* CaptureTexture{nullptr};
        SDL_Texture* PreviousTarget{nullptr};
        Point        PreviousOriginOffsetLogical{};
        Rect         SubjectRectLogical{};
        Transform    AppliedTransform{};
        float        Opacity{1.0f};
    };

    SDL_Renderer*         SdlRenderer{nullptr};
    IFontBackend*         FontBackend{nullptr};
    float                 DpiScaleFactor{1.0f};
    std::uint64_t         FrameIndex{0};
    float                 FrameDeltaSeconds{0.0f};
    std::uint64_t         LastBeginFrameTicksNs{0}; // for the one-argument BeginFrame
    std::vector<SDL_Rect>      ClipStack;  // physical-pixel rects, already intersected
    std::vector<SDL_BlendMode> BlendStack; // resolved SDL blend modes -- PushBlendMode and
                                            // PushCustomBlendMode both push onto this one stack

    // Subtracted from every logical coordinate before DPI scaling. Zero except while a
    // PushTransform capture is active, when it equals that capture's SubjectRectLogical
    // origin -- so widgets keep drawing at their normal absolute logical coordinates
    // without knowing they've been redirected to a small offscreen texture.
    Point                       OriginOffsetLogical{};
    std::vector<TransformFrame> TransformStack;
};

} // namespace Penumbra::Render
