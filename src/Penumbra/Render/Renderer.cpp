#include "Penumbra/Render/Renderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace Penumbra::Render {

namespace {

constexpr float Pi = 3.14159265358979323846f;

// Width in physical pixels of the anti-aliasing fringe; it straddles the edge.
constexpr float AaFeatherPhysical = 1.0f;

SDL_Color ToSdlColor(Color InColor) {
    return {InColor.R, InColor.G, InColor.B, InColor.A};
}

SDL_FColor ToFColor(Color InColor) {
    return {InColor.R / 255.0f, InColor.G / 255.0f, InColor.B / 255.0f, InColor.A / 255.0f};
}

// More segments per corner as the radius grows, so small radii stay cheap and large
// ones stay smooth.
int SegmentsForRadius(float RadiusPhysical) {
    return std::clamp(static_cast<int>(std::ceil(RadiusPhysical / 2.0f)), 2, 48);
}

// A rounded-rect perimeter (clockwise, screen space y-down) plus the outward unit
// normal at each point — the normal is just the corner's radial direction, which at
// the arc endpoints is axis-aligned and so matches the adjacent straight edge too.
struct RoundedRing {
    std::vector<SDL_FPoint> Points;
    std::vector<SDL_FPoint> Normals;
};

RoundedRing BuildRoundedRing(SDL_FRect R, float Radius, int Segments) {
    struct Corner {
        float CentreX, CentreY, AngleStart, AngleEnd;
    };
    const Corner Corners[4] = {
        {R.x + Radius,          R.y + Radius,          Pi,          Pi * 1.5f}, // top-left
        {R.x + R.w - Radius,    R.y + Radius,          Pi * 1.5f,   Pi * 2.0f}, // top-right
        {R.x + R.w - Radius,    R.y + R.h - Radius,    0.0f,        Pi * 0.5f}, // bottom-right
        {R.x + Radius,          R.y + R.h - Radius,    Pi * 0.5f,   Pi},        // bottom-left
    };

    RoundedRing Ring;
    Ring.Points.reserve(static_cast<std::size_t>(Segments + 1) * 4);
    Ring.Normals.reserve(static_cast<std::size_t>(Segments + 1) * 4);
    for (const Corner& C : Corners) {
        for (int Step = 0; Step <= Segments; ++Step) {
            const float T = static_cast<float>(Step) / static_cast<float>(Segments);
            const float Angle = C.AngleStart + (C.AngleEnd - C.AngleStart) * T;
            const float CosA = std::cos(Angle);
            const float SinA = std::sin(Angle);
            Ring.Points.push_back({C.CentreX + Radius * CosA, C.CentreY + Radius * SinA});
            Ring.Normals.push_back({CosA, SinA});
        }
    }
    return Ring;
}

// A full-circle ring (Segments points around the whole circumference) plus the
// outward unit normal at each point -- the filled-disc analogue of
// BuildRoundedRing's per-corner quarter arcs, used by DrawRadialGradient.
RoundedRing BuildCircleRing(SDL_FPoint Centre, float Radius, int Segments) {
    RoundedRing Ring;
    Ring.Points.reserve(static_cast<std::size_t>(Segments));
    Ring.Normals.reserve(static_cast<std::size_t>(Segments));
    for (int Step = 0; Step < Segments; ++Step) {
        const float Angle = (2.0f * Pi) * static_cast<float>(Step) / static_cast<float>(Segments);
        const float CosA = std::cos(Angle);
        const float SinA = std::sin(Angle);
        Ring.Points.push_back({Centre.x + Radius * CosA, Centre.y + Radius * SinA});
        Ring.Normals.push_back({CosA, SinA});
    }
    return Ring;
}

} // namespace

bool Renderer::Initialise(SDL_Renderer* InSdlRenderer, float InDpiScaleFactor,
                          IFontBackend* InFontBackend) {
    if (!InSdlRenderer) {
        return false;
    }
    SdlRenderer = InSdlRenderer;
    DpiScaleFactor = (InDpiScaleFactor > 0.0f) ? InDpiScaleFactor : 1.0f;
    FontBackend = InFontBackend;
    // Alpha blending is required for the anti-aliasing fringe (zero-alpha vertices)
    // to fade against what is behind it.
    SDL_SetRenderDrawBlendMode(SdlRenderer, SDL_BLENDMODE_BLEND);
    return true;
}

Point Renderer::ToPhysicalPoint(Point PointLogical) const {
    return {(PointLogical.X - OriginOffsetLogical.X) * DpiScaleFactor,
            (PointLogical.Y - OriginOffsetLogical.Y) * DpiScaleFactor};
}

SDL_FRect Renderer::ToPhysical(Rect RectLogical) const {
    const Point Origin = ToPhysicalPoint({RectLogical.X, RectLogical.Y});
    return {Origin.X, Origin.Y, RectLogical.W * DpiScaleFactor, RectLogical.H * DpiScaleFactor};
}

void Renderer::BeginFrame(Color ClearColor) {
    SDL_SetRenderDrawColor(SdlRenderer, ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A);
    SDL_RenderClear(SdlRenderer);
}

void Renderer::EndFrameAndPresent() {
    assert(ClipStack.empty() && "clip stack not balanced: a PushClipRect is missing its PopClipRect");
    assert(BlendStack.empty() && "blend stack not balanced: a PushBlendMode is missing its PopBlendMode");
    assert(TransformStack.empty() && "transform stack not balanced: a PushTransform is missing its PopTransform");
    SDL_RenderPresent(SdlRenderer);
}

void Renderer::DrawFilledRect(Rect RectLogical, Color InColor, float CornerRadiusLogical) {
    const SDL_FRect R = ToPhysical(RectLogical);
    float Radius = CornerRadiusLogical * DpiScaleFactor;

    if (Radius <= 0.0f) {
        SDL_SetRenderDrawColor(SdlRenderer, InColor.R, InColor.G, InColor.B, InColor.A);
        SDL_RenderFillRect(SdlRenderer, &R);
        return;
    }

    Radius = std::min(Radius, std::min(R.w, R.h) * 0.5f);
    const int Segments = SegmentsForRadius(Radius);
    const RoundedRing Ring = BuildRoundedRing(R, Radius, Segments);
    const int Count = static_cast<int>(Ring.Points.size());
    const float Half = AaFeatherPhysical * 0.5f;

    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    // A solid interior fan, ringed by a fringe that fades to zero alpha. The fringe
    // straddles the true edge (±half a feather along the normal), smoothing the arcs.
    std::vector<SDL_Vertex> Vertices;
    Vertices.reserve(static_cast<std::size_t>(1 + 2 * Count));
    Vertices.push_back({{R.x + R.w * 0.5f, R.y + R.h * 0.5f}, Solid, {0.0f, 0.0f}}); // centre
    for (int Index = 0; Index < Count; ++Index) {                                   // inner: solid
        const SDL_FPoint P = Ring.Points[Index];
        const SDL_FPoint Nrm = Ring.Normals[Index];
        Vertices.push_back({{P.x - Nrm.x * Half, P.y - Nrm.y * Half}, Solid, {0.0f, 0.0f}});
    }
    for (int Index = 0; Index < Count; ++Index) {                                   // outer: clear
        const SDL_FPoint P = Ring.Points[Index];
        const SDL_FPoint Nrm = Ring.Normals[Index];
        Vertices.push_back({{P.x + Nrm.x * Half, P.y + Nrm.y * Half}, Clear, {0.0f, 0.0f}});
    }

    const int InnerBase = 1;
    const int OuterBase = 1 + Count;
    std::vector<int> Indices;
    Indices.reserve(static_cast<std::size_t>(Count) * 9);
    for (int Index = 0; Index < Count; ++Index) {
        const int A = Index;
        const int B = (Index + 1) % Count;
        Indices.push_back(0);            Indices.push_back(InnerBase + A); Indices.push_back(InnerBase + B);
        Indices.push_back(InnerBase + A); Indices.push_back(InnerBase + B); Indices.push_back(OuterBase + B);
        Indices.push_back(InnerBase + A); Indices.push_back(OuterBase + B); Indices.push_back(OuterBase + A);
    }

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawGradientRect(Rect RectLogical, Color TopColor, Color BottomColor,
                                float CornerRadiusLogical) {
    const SDL_FRect R = ToPhysical(RectLogical);
    float Radius = CornerRadiusLogical * DpiScaleFactor;

    const SDL_FColor Top = ToFColor(TopColor);
    const SDL_FColor Bottom = ToFColor(BottomColor);
    auto LerpAtY = [&](float Y) {
        const float T = (R.h > 0.0f) ? std::clamp((Y - R.y) / R.h, 0.0f, 1.0f) : 0.0f;
        return SDL_FColor{Top.r + (Bottom.r - Top.r) * T, Top.g + (Bottom.g - Top.g) * T,
                          Top.b + (Bottom.b - Top.b) * T, Top.a + (Bottom.a - Top.a) * T};
    };

    if (Radius <= 0.0f) {
        // No per-vertex color in the SDL_RenderFillRect fast path DrawFilledRect uses,
        // so a flat gradient always takes the SDL_RenderGeometry route (4 vertices, one
        // flat quad) even at zero radius.
        const SDL_Vertex Vertices[4] = {
            {{R.x,         R.y},         Top,    {0.0f, 0.0f}},
            {{R.x + R.w,   R.y},         Top,    {0.0f, 0.0f}},
            {{R.x + R.w,   R.y + R.h},   Bottom, {0.0f, 0.0f}},
            {{R.x,         R.y + R.h},   Bottom, {0.0f, 0.0f}},
        };
        const int Indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(SdlRenderer, nullptr, Vertices, 4, Indices, 6);
        return;
    }

    Radius = std::min(Radius, std::min(R.w, R.h) * 0.5f);
    const int Segments = SegmentsForRadius(Radius);
    const RoundedRing Ring = BuildRoundedRing(R, Radius, Segments);
    const int Count = static_cast<int>(Ring.Points.size());
    const float Half = AaFeatherPhysical * 0.5f;

    // Same solid-centre-fan-plus-fading-ring structure as DrawFilledRect, except each
    // vertex's color is looked up by its own Y position instead of being one flat Solid.
    std::vector<SDL_Vertex> Vertices;
    Vertices.reserve(static_cast<std::size_t>(1 + 2 * Count));
    const SDL_FPoint Centre{R.x + R.w * 0.5f, R.y + R.h * 0.5f};
    Vertices.push_back({Centre, LerpAtY(Centre.y), {0.0f, 0.0f}});
    for (int Index = 0; Index < Count; ++Index) { // inner: solid, colored by position
        const SDL_FPoint P = Ring.Points[Index];
        const SDL_FPoint Nrm = Ring.Normals[Index];
        const SDL_FPoint Inner{P.x - Nrm.x * Half, P.y - Nrm.y * Half};
        Vertices.push_back({Inner, LerpAtY(Inner.y), {0.0f, 0.0f}});
    }
    for (int Index = 0; Index < Count; ++Index) { // outer: same color, fading to zero alpha
        const SDL_FPoint P = Ring.Points[Index];
        const SDL_FPoint Nrm = Ring.Normals[Index];
        const SDL_FPoint Outer{P.x + Nrm.x * Half, P.y + Nrm.y * Half};
        SDL_FColor OuterColor = LerpAtY(Outer.y);
        OuterColor.a = 0.0f;
        Vertices.push_back({Outer, OuterColor, {0.0f, 0.0f}});
    }

    const int InnerBase = 1;
    const int OuterBase = 1 + Count;
    std::vector<int> Indices;
    Indices.reserve(static_cast<std::size_t>(Count) * 9);
    for (int Index = 0; Index < Count; ++Index) {
        const int A = Index;
        const int B = (Index + 1) % Count;
        Indices.push_back(0);            Indices.push_back(InnerBase + A); Indices.push_back(InnerBase + B);
        Indices.push_back(InnerBase + A); Indices.push_back(InnerBase + B); Indices.push_back(OuterBase + B);
        Indices.push_back(InnerBase + A); Indices.push_back(OuterBase + B); Indices.push_back(OuterBase + A);
    }

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawRadialGradient(Point Centre, float RadiusLogical, Color CentreColor, Color EdgeColor) {
    DrawRadialGradient(Centre, RadiusLogical,
                       std::vector<GradientStop>{{0.0f, CentreColor}, {1.0f, EdgeColor}});
}

void Renderer::DrawRadialGradient(Point Centre, float RadiusLogical, const std::vector<GradientStop>& Stops) {
    if (RadiusLogical <= 0.0f || Stops.size() < 2) {
        return;
    }

    const Point CentrePhysical = ToPhysicalPoint(Centre);
    const SDL_FPoint C{CentrePhysical.X, CentrePhysical.Y};
    const float Radius = RadiusLogical * DpiScaleFactor;
    const float Half = AaFeatherPhysical * 0.5f;

    // Segment count scales the same way SegmentsForRadius already does for one 90-degree
    // corner arc, just carried across the full 360-degree circumference (4x), so a disc
    // gets the same angular resolution a rounded-rect corner of the same radius would.
    const int Segments = 4 * SegmentsForRadius(Radius);

    // A centre vertex plus one concentric ring per stop after the first, banded together
    // with the same solid-centre-fan-plus-fading-ring structure the 2-color overload
    // (and DrawFilledRect/DrawGradientRect before it) already uses -- triangle
    // rasterisation supplies the interpolation between each adjacent pair of stop colors
    // for free, the same way it does between the centre and a single edge color. Only the
    // last stop's ring is inset by Half (straddled by the AA feather ring appended after
    // the loop); interior stop-to-stop rings need no feather, since they're not an edge
    // against the destination background.
    std::vector<SDL_Vertex> Vertices;
    std::vector<int> Indices;
    Vertices.reserve(static_cast<std::size_t>(1 + Stops.size() * Segments));

    Vertices.push_back({C, ToFColor(Stops.front().StopColor), {0.0f, 0.0f}});

    auto AddBand = [&](int PrevBase, int PrevCount, int RingBase) {
        for (int Index = 0; Index < Segments; ++Index) {
            const int A = Index;
            const int B = (Index + 1) % Segments;
            if (PrevCount == 1) { // fan out from the single centre vertex
                Indices.push_back(PrevBase);
                Indices.push_back(RingBase + A);
                Indices.push_back(RingBase + B);
            } else {
                Indices.push_back(PrevBase + A); Indices.push_back(PrevBase + B); Indices.push_back(RingBase + B);
                Indices.push_back(PrevBase + A); Indices.push_back(RingBase + B); Indices.push_back(RingBase + A);
            }
        }
    };

    int PrevBase = 0;
    int PrevCount = 1; // the centre vertex
    for (std::size_t Index = 1; Index < Stops.size(); ++Index) {
        const bool IsLast = (Index + 1 == Stops.size());
        const float T = std::clamp(Stops[Index].T, 0.0f, 1.0f);
        const float StopRadius = IsLast ? std::max(0.0f, Radius * T - Half) : Radius * T;
        const RoundedRing Ring = BuildCircleRing(C, StopRadius, Segments);
        const SDL_FColor StopColor = ToFColor(Stops[Index].StopColor);
        const int RingBase = static_cast<int>(Vertices.size());
        for (int Seg = 0; Seg < Segments; ++Seg) {
            Vertices.push_back({Ring.Points[Seg], StopColor, {0.0f, 0.0f}});
        }
        AddBand(PrevBase, PrevCount, RingBase);
        PrevBase = RingBase;
        PrevCount = Segments;
    }

    { // AA feather: fade the last stop's color to zero alpha, Half beyond its true radius.
        SDL_FColor EdgeClear = ToFColor(Stops.back().StopColor);
        EdgeClear.a = 0.0f;
        const float T = std::clamp(Stops.back().T, 0.0f, 1.0f);
        const RoundedRing Ring = BuildCircleRing(C, Radius * T + Half, Segments);
        const int RingBase = static_cast<int>(Vertices.size());
        for (int Seg = 0; Seg < Segments; ++Seg) {
            Vertices.push_back({Ring.Points[Seg], EdgeClear, {0.0f, 0.0f}});
        }
        AddBand(PrevBase, PrevCount, RingBase);
    }

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawRectOutline(Rect RectLogical, Color InColor, float ThicknessLogical,
                               float CornerRadiusLogical) {
    const SDL_FRect R = ToPhysical(RectLogical);
    const float T = ThicknessLogical * DpiScaleFactor;
    float Radius = CornerRadiusLogical * DpiScaleFactor;

    if (Radius <= 0.0f) {
        SDL_SetRenderDrawColor(SdlRenderer, InColor.R, InColor.G, InColor.B, InColor.A);
        const SDL_FRect Edges[4] = {
            {R.x,           R.y,           R.w, T  }, // top
            {R.x,           R.y + R.h - T, R.w, T  }, // bottom
            {R.x,           R.y,           T,   R.h}, // left
            {R.x + R.w - T, R.y,           T,   R.h}, // right
        };
        for (const SDL_FRect& Edge : Edges) {
            SDL_RenderFillRect(SdlRenderer, &Edge);
        }
        return;
    }

    Radius = std::min(Radius, std::min(R.w, R.h) * 0.5f);

    // A border thicker than the box collapses to a solid rounded fill.
    const SDL_FRect Inner{R.x + T, R.y + T, R.w - 2.0f * T, R.h - 2.0f * T};
    if (Inner.w <= 0.0f || Inner.h <= 0.0f) {
        DrawFilledRect(RectLogical, InColor, CornerRadiusLogical);
        return;
    }

    const int Segments = SegmentsForRadius(Radius);
    const float InnerRadius = std::max(0.0f, Radius - T);
    // Outer and inner perimeters share corner centres and angles (inset by T, radius
    // shrinks by T), so they pair up one-to-one; normals are common to both.
    const RoundedRing Outer = BuildRoundedRing(R, Radius, Segments);
    const RoundedRing InnerR = BuildRoundedRing(Inner, InnerRadius, Segments);
    const int Count = static_cast<int>(Outer.Points.size());
    const float Half = AaFeatherPhysical * 0.5f;

    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    // Four concentric rings: the solid border core (outer-core → inner-core) with a
    // fading fringe on each side. Pushing +normal grows the radius, -normal shrinks it.
    std::vector<SDL_Vertex> Vertices;
    Vertices.reserve(static_cast<std::size_t>(4 * Count));
    auto PushRing = [&](const RoundedRing& Ring, float Sign, const SDL_FColor& VertexColor) {
        for (int Index = 0; Index < Count; ++Index) {
            const SDL_FPoint P = Ring.Points[Index];
            const SDL_FPoint Nrm = Ring.Normals[Index];
            Vertices.push_back({{P.x + Nrm.x * Half * Sign, P.y + Nrm.y * Half * Sign},
                                VertexColor, {0.0f, 0.0f}});
        }
    };
    PushRing(Outer,  1.0f, Clear);  // outer fade
    PushRing(Outer, -1.0f, Solid);  // outer core
    PushRing(InnerR, 1.0f, Solid);  // inner core
    PushRing(InnerR, -1.0f, Clear); // inner fade

    std::vector<int> Indices;
    Indices.reserve(static_cast<std::size_t>(Count) * 18);
    auto AddBand = [&](int BaseA, int BaseB) {
        for (int Index = 0; Index < Count; ++Index) {
            const int A = Index;
            const int B = (Index + 1) % Count;
            Indices.push_back(BaseA + A); Indices.push_back(BaseA + B); Indices.push_back(BaseB + B);
            Indices.push_back(BaseA + A); Indices.push_back(BaseB + B); Indices.push_back(BaseB + A);
        }
    };
    AddBand(0 * Count, 1 * Count); // outer fringe (fade → core)
    AddBand(1 * Count, 2 * Count); // solid core
    AddBand(2 * Count, 3 * Count); // inner fringe (core → fade)

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawDropShadow(Rect RectLogical, Color ShadowColor, float BlurRadiusLogical,
                              float CornerRadiusLogical) {
    if (BlurRadiusLogical <= 0.0f) {
        return;
    }

    const SDL_FRect R = ToPhysical(RectLogical);
    float Radius = CornerRadiusLogical * DpiScaleFactor;
    Radius = std::min(Radius, std::min(R.w, R.h) * 0.5f);
    const float Blur = BlurRadiusLogical * DpiScaleFactor;

    // The inner ring sits exactly on the caster's own edge (solid alpha); the widget
    // itself is drawn on top afterwards and covers the interior, so no centre fill is
    // needed here -- just the ring, fading outward over Blur instead of the fixed 1px
    // AA feather.
    const int Segments = SegmentsForRadius(Radius + Blur);
    const RoundedRing Ring = BuildRoundedRing(R, Radius, Segments);
    const int Count = static_cast<int>(Ring.Points.size());

    const SDL_FColor Solid = ToFColor(ShadowColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    std::vector<SDL_Vertex> Vertices;
    Vertices.reserve(static_cast<std::size_t>(2 * Count));
    for (int Index = 0; Index < Count; ++Index) { // inner: solid, at the caster's edge
        Vertices.push_back({Ring.Points[Index], Solid, {0.0f, 0.0f}});
    }
    for (int Index = 0; Index < Count; ++Index) { // outer: fades to transparent, Blur out
        const SDL_FPoint P = Ring.Points[Index];
        const SDL_FPoint Nrm = Ring.Normals[Index];
        Vertices.push_back({{P.x + Nrm.x * Blur, P.y + Nrm.y * Blur}, Clear, {0.0f, 0.0f}});
    }

    const int OuterBase = Count;
    std::vector<int> Indices;
    Indices.reserve(static_cast<std::size_t>(Count) * 6);
    for (int Index = 0; Index < Count; ++Index) {
        const int A = Index;
        const int B = (Index + 1) % Count;
        Indices.push_back(A); Indices.push_back(B); Indices.push_back(OuterBase + B);
        Indices.push_back(A); Indices.push_back(OuterBase + B); Indices.push_back(OuterBase + A);
    }

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawLine(Point From, Point To, Color InColor, float ThicknessLogical) {
    const Point FromPhysical = ToPhysicalPoint(From);
    const Point ToPhysicalP  = ToPhysicalPoint(To);
    const SDL_FPoint P0{FromPhysical.X, FromPhysical.Y};
    const SDL_FPoint P1{ToPhysicalP.X, ToPhysicalP.Y};
    const float Thickness = ThicknessLogical * DpiScaleFactor;

    float DirX = P1.x - P0.x;
    float DirY = P1.y - P0.y;
    const float Length = std::sqrt(DirX * DirX + DirY * DirY);
    if (Length <= 0.0f || Thickness <= 0.0f) {
        return;
    }
    DirX /= Length;
    DirY /= Length;
    const float PerpX = -DirY;
    const float PerpY = DirX;

    const float HalfThickness = Thickness * 0.5f;
    const float Half = AaFeatherPhysical * 0.5f;

    // Extend the core along the line's own axis by Half so the solid region still
    // reaches the true endpoints once the perpendicular fringe eats into it -- same
    // edge-straddling idea as DrawRectOutline's feather, just not re-faded at the caps.
    const SDL_FPoint CoreStart{P0.x - DirX * Half, P0.y - DirY * Half};
    const SDL_FPoint CoreEnd{P1.x + DirX * Half, P1.y + DirY * Half};

    auto Offset = [&](SDL_FPoint P, float PerpScale) {
        return SDL_FPoint{P.x + PerpX * PerpScale, P.y + PerpY * PerpScale};
    };

    const SDL_FPoint Inner[4] = {
        Offset(CoreStart, HalfThickness),  Offset(CoreEnd, HalfThickness),
        Offset(CoreEnd, -HalfThickness),   Offset(CoreStart, -HalfThickness),
    };
    const SDL_FPoint Outer[4] = {
        Offset(CoreStart, HalfThickness + Half), Offset(CoreEnd, HalfThickness + Half),
        Offset(CoreEnd, -HalfThickness - Half),  Offset(CoreStart, -HalfThickness - Half),
    };

    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    SDL_Vertex Vertices[8];
    for (int Index = 0; Index < 4; ++Index) {
        Vertices[Index]     = {Inner[Index], Solid, {0.0f, 0.0f}};
        Vertices[4 + Index] = {Outer[Index], Clear, {0.0f, 0.0f}};
    }

    // Solid core, plus a fading fringe band on each of the two long (thickness) edges.
    const int Indices[] = {
        0, 1, 2,  0, 2, 3,
        0, 1, 5,  0, 5, 4,
        2, 3, 7,  2, 7, 6,
    };

    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices, 8, Indices, 18);
}

void Renderer::DrawTriangleFilled(Point A, Point B, Point C, Color InColor) {
    const SDL_FColor Solid = ToFColor(InColor);
    const Point Ap = ToPhysicalPoint(A);
    const Point Bp = ToPhysicalPoint(B);
    const Point Cp = ToPhysicalPoint(C);
    const SDL_Vertex Vertices[3] = {
        {{Ap.X, Ap.Y}, Solid, {0.0f, 0.0f}},
        {{Bp.X, Bp.Y}, Solid, {0.0f, 0.0f}},
        {{Cp.X, Cp.Y}, Solid, {0.0f, 0.0f}},
    };
    const int Indices[3] = {0, 1, 2};
    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices, 3, Indices, 3);
}

void Renderer::DrawText(FontHandle Font, std::string_view Text, Point PositionLogical,
                        Color InColor) {
    if (!FontBackend || Text.empty()) {
        return;
    }

    SDL_Texture* Texture = FontBackend->AcquireTextTexture(SdlRenderer, Font, Text, ToSdlColor(InColor));
    if (!Texture) {
        return;
    }

    // Glyph texture is already at physical pixel size; only the position scales.
    float TextureWidth = 0.0f;
    float TextureHeight = 0.0f;
    SDL_GetTextureSize(Texture, &TextureWidth, &TextureHeight);

    const Point PositionPhysical = ToPhysicalPoint(PositionLogical);
    const SDL_FRect Destination{std::round(PositionPhysical.X), std::round(PositionPhysical.Y),
                                TextureWidth, TextureHeight};
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}

SDL_Renderer* Renderer::GetSdlRenderer() const {
    return SdlRenderer;
}

void Renderer::DrawTexture(SDL_Texture* Texture, Rect DestLogical) {
    if (!Texture) {
        return;
    }
    // The scene texture is sized in physical pixels (content × dpi); rounding the
    // destination size to match makes the blit genuinely 1:1 — no resample.
    SDL_FRect Destination = ToPhysical(DestLogical);
    Destination.w = std::round(Destination.w);
    Destination.h = std::round(Destination.h);
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}

void Renderer::PushClipRect(Rect RectLogical) {
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

void Renderer::PushBlendMode(BlendMode Mode) {
    const SDL_BlendMode Sdl = (Mode == BlendMode::Additive) ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND;
    BlendStack.push_back(Sdl);
    SDL_SetRenderDrawBlendMode(SdlRenderer, Sdl);
}

void Renderer::PushCustomBlendMode(SDL_BlendFactor SrcColorFactor, SDL_BlendFactor DstColorFactor,
                                   SDL_BlendOperation ColorOperation, SDL_BlendFactor SrcAlphaFactor,
                                   SDL_BlendFactor DstAlphaFactor, SDL_BlendOperation AlphaOperation) {
    const SDL_BlendMode Custom = SDL_ComposeCustomBlendMode(SrcColorFactor, DstColorFactor, ColorOperation,
                                                             SrcAlphaFactor, DstAlphaFactor, AlphaOperation);
    BlendStack.push_back(Custom);
    SDL_SetRenderDrawBlendMode(SdlRenderer, Custom);
}

void Renderer::PopBlendMode() {
    assert(!BlendStack.empty() && "PopBlendMode called with an empty blend stack");
    BlendStack.pop_back();
    const SDL_BlendMode Restored = BlendStack.empty() ? SDL_BLENDMODE_BLEND : BlendStack.back();
    SDL_SetRenderDrawBlendMode(SdlRenderer, Restored);
}

void Renderer::PushTransform(Rect SubjectRectLogical, const Transform& InTransform) {
    if (InTransform.IsIdentity()) {
        TransformStack.push_back({nullptr, nullptr, OriginOffsetLogical, SubjectRectLogical, InTransform});
        return;
    }

    const int PhysicalWidth  = std::max(1, static_cast<int>(std::ceil(SubjectRectLogical.W * DpiScaleFactor)));
    const int PhysicalHeight = std::max(1, static_cast<int>(std::ceil(SubjectRectLogical.H * DpiScaleFactor)));

    SDL_Texture* CaptureTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_RGBA8888,
                                                     SDL_TEXTUREACCESS_TARGET, PhysicalWidth, PhysicalHeight);
    SDL_SetTextureBlendMode(CaptureTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(CaptureTexture, SDL_SCALEMODE_LINEAR);

    TransformStack.push_back({CaptureTexture, SDL_GetRenderTarget(SdlRenderer), OriginOffsetLogical,
                              SubjectRectLogical, InTransform});

    // The captured texture is its own render target with its own clip state; the
    // outer target's clip (e.g. a ScrollablePanel ancestor) doesn't apply to it in any
    // coordinate space that would make sense here, so it's cleared for the capture and
    // restored verbatim in PopTransform once the target switches back.
    SDL_SetRenderClipRect(SdlRenderer, nullptr);
    SDL_SetRenderTarget(SdlRenderer, CaptureTexture);
    OriginOffsetLogical = {SubjectRectLogical.X, SubjectRectLogical.Y};

    SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 0);
    SDL_RenderClear(SdlRenderer);
}

void Renderer::PopTransform() {
    assert(!TransformStack.empty() && "PopTransform with no matching PushTransform");
    const TransformFrame Frame = TransformStack.back();
    TransformStack.pop_back();

    if (!Frame.CaptureTexture) {
        return; // identity push -- drawing was never redirected
    }

    SDL_SetRenderTarget(SdlRenderer, Frame.PreviousTarget);
    OriginOffsetLogical = Frame.PreviousOriginOffsetLogical;
    SDL_SetRenderClipRect(SdlRenderer, ClipStack.empty() ? nullptr : &ClipStack.back());

    // Destination size folds in Scale, destination position folds in Translate;
    // SDL_RenderTextureRotated's own angle/centre params handle Rotation around the
    // subject's own centre -- exactly the transform-origin: centre semantics
    // BoxStyle::Transform documents.
    const Transform& T       = Frame.AppliedTransform;
    const Rect&       Subject = Frame.SubjectRectLogical;

    const float DestWidth  = Subject.W * T.ScaleX * DpiScaleFactor;
    const float DestHeight = Subject.H * T.ScaleY * DpiScaleFactor;
    const Point CentreLogical{Subject.X + Subject.W * 0.5f + T.TranslateXLogical,
                              Subject.Y + Subject.H * 0.5f + T.TranslateYLogical};
    const Point CentrePhysical = ToPhysicalPoint(CentreLogical);

    const SDL_FRect Destination{CentrePhysical.X - DestWidth * 0.5f, CentrePhysical.Y - DestHeight * 0.5f,
                                DestWidth, DestHeight};
    const SDL_FPoint Pivot{DestWidth * 0.5f, DestHeight * 0.5f};

    SDL_RenderTextureRotated(SdlRenderer, Frame.CaptureTexture, nullptr, &Destination, T.RotationDegrees,
                             &Pivot, SDL_FLIP_NONE);
    SDL_DestroyTexture(Frame.CaptureTexture);
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

void Renderer::SetDpiScaleFactor(float NewScaleFactor) {
    DpiScaleFactor = (NewScaleFactor > 0.0f) ? NewScaleFactor : DpiScaleFactor;
}

} // namespace Penumbra::Render
