#include "Penumbra/Render/Renderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Penumbra::Render {

namespace {

constexpr float Pi = 3.14159265358979323846f;

// Width in physical pixels of the anti-aliasing fringe; it straddles the edge.
constexpr float AaFeatherPhysical = 1.0f;

// SVG's default stroke-miterlimit: the longest a miter corner may reach, as a multiple
// of the line's thickness, before DrawPolyline bevels it instead. DrawFilledPolygon
// caps its fringe's corners at the same length.
constexpr float MiterLimit = 4.0f;

// Consecutive path points closer than this, in physical pixels, count as one point.
constexpr float MinPointSpacingPhysical = 0.01f;

// The longest frame the frame clock reports, matching PlatformWindow's clamp on the
// input's DeltaTimeSeconds, so a stalled frame doesn't make every animation jump.
constexpr float MaxFrameDeltaSeconds = 0.1f;

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

SDL_FPoint operator+(SDL_FPoint A, SDL_FPoint B) { return {A.x + B.x, A.y + B.y}; }
SDL_FPoint operator-(SDL_FPoint A, SDL_FPoint B) { return {A.x - B.x, A.y - B.y}; }
SDL_FPoint operator-(SDL_FPoint A) { return {-A.x, -A.y}; }
SDL_FPoint operator*(SDL_FPoint A, float Scale) { return {A.x * Scale, A.y * Scale}; }

float Dot(SDL_FPoint A, SDL_FPoint B) { return A.x * B.x + A.y * B.y; }
float Cross(SDL_FPoint A, SDL_FPoint B) { return A.x * B.y - A.y * B.x; }
float Length(SDL_FPoint A) { return std::sqrt(Dot(A, A)); }

// A direction turned a quarter turn clockwise on screen (y runs down).
SDL_FPoint Normal(SDL_FPoint Direction) { return {-Direction.y, Direction.x}; }

SDL_FPoint Rotated(SDL_FPoint A, float Radians) {
    const float CosA = std::cos(Radians);
    const float SinA = std::sin(Radians);
    return {A.x * CosA - A.y * SinA, A.x * SinA + A.y * CosA};
}

// Removes the zero-area triangles a strip leaves wherever two of its vertices land on
// the same spot: a pie's centre, or the inside of a polyline's corner.
void DropDegenerateTriangles(const std::vector<SDL_Vertex>& Vertices, std::vector<int>& Indices) {
    std::size_t Kept = 0;
    for (std::size_t Index = 0; Index + 2 < Indices.size(); Index += 3) {
        const SDL_FPoint A = Vertices[static_cast<std::size_t>(Indices[Index])].position;
        const SDL_FPoint B = Vertices[static_cast<std::size_t>(Indices[Index + 1])].position;
        const SDL_FPoint C = Vertices[static_cast<std::size_t>(Indices[Index + 2])].position;
        if (std::abs(Cross(B - A, C - A)) > 1e-6f) {
            std::copy_n(Indices.begin() + static_cast<std::ptrdiff_t>(Index), 3,
                        Indices.begin() + static_cast<std::ptrdiff_t>(Kept));
            Kept += 3;
        }
    }
    Indices.resize(Kept);
}

// Triangulates a polygon whose edges don't cross by ear clipping, returning index
// triples into Path. Winding is the sign of Path's signed area. If no ear can be found
// (the edges do cross after all, or every corner left is a straight line) the rest is
// fanned out, so a bad polygon still draws something rather than nothing.
std::vector<int> TriangulatePolygon(const std::vector<SDL_FPoint>& Path, float Winding) {
    std::vector<int> Remaining(Path.size());
    for (std::size_t Index = 0; Index < Remaining.size(); ++Index) {
        Remaining[Index] = static_cast<int>(Index);
    }
    std::vector<int> Triangles;
    Triangles.reserve(3 * (Path.size() - 2));

    // Positive when A -> B -> C turns the same way the whole polygon winds.
    auto Turn = [&](int A, int B, int C) { return Winding * Cross(Path[B] - Path[A], Path[C] - Path[A]); };
    auto Neighbours = [&](std::size_t At) {
        const std::size_t Size = Remaining.size();
        return std::array<int, 3>{Remaining[(At + Size - 1) % Size], Remaining[At], Remaining[(At + 1) % Size]};
    };
    auto IsEar = [&](std::size_t At) {
        const auto [A, B, C] = Neighbours(At);
        if (Turn(A, B, C) <= 0.0f) {
            return false;
        }
        for (const int Other : Remaining) {
            if (Other != A && Other != B && Other != C && Turn(A, B, Other) > 0.0f && Turn(B, C, Other) > 0.0f &&
                Turn(C, A, Other) > 0.0f) {
                return false;
            }
        }
        return true;
    };

    std::size_t At = 0;
    std::size_t Misses = 0;
    while (Remaining.size() > 3 && Misses < Remaining.size()) {
        if (IsEar(At)) {
            const auto [A, B, C] = Neighbours(At);
            Triangles.insert(Triangles.end(), {A, B, C});
            Remaining.erase(Remaining.begin() + static_cast<std::ptrdiff_t>(At));
            // Removing B can turn A into an ear, so look there next.
            At = (At + Remaining.size() - 1) % Remaining.size();
            Misses = 0;
        } else {
            At = (At + 1) % Remaining.size();
            ++Misses;
        }
    }
    for (std::size_t Index = 1; Index + 1 < Remaining.size(); ++Index) {
        Triangles.insert(Triangles.end(), {Remaining[0], Remaining[Index], Remaining[Index + 1]});
    }
    return Triangles;
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

std::vector<SDL_FPoint> Renderer::ToPhysicalPath(std::span<const Point> PointsLogical, bool IsClosed) const {
    std::vector<SDL_FPoint> Path;
    Path.reserve(PointsLogical.size());
    for (const Point& PointLogical : PointsLogical) {
        const Point Physical = ToPhysicalPoint(PointLogical);
        const SDL_FPoint Next{Physical.X, Physical.Y};
        if (Path.empty() || Length(Next - Path.back()) > MinPointSpacingPhysical) {
            Path.push_back(Next);
        }
    }
    if (IsClosed && Path.size() > 1 && Length(Path.front() - Path.back()) <= MinPointSpacingPhysical) {
        Path.pop_back();
    }
    return Path;
}

void Renderer::BeginFrame(Color ClearColor) {
    const std::uint64_t Now = SDL_GetTicksNS();
    const float Delta =
        LastBeginFrameTicksNs == 0 ? 0.0f : static_cast<float>(static_cast<double>(Now - LastBeginFrameTicksNs) / 1.0e9);
    LastBeginFrameTicksNs = Now;
    BeginFrame(ClearColor, Delta);
}

void Renderer::BeginFrame(Color ClearColor, float InFrameDeltaSeconds) {
    ++FrameIndex;
    FrameDeltaSeconds = std::clamp(InFrameDeltaSeconds, 0.0f, MaxFrameDeltaSeconds);

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
    if (R.w <= 0.0f || R.h <= 0.0f) {
        return; // no area, so no fill; the AA fringe alone would paint a sliver
    }
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
    if (R.w <= 0.0f || R.h <= 0.0f) {
        return; // as DrawFilledRect
    }
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
    if (R.w <= 0.0f || R.h <= 0.0f) {
        return; // as DrawFilledRect; the square path would otherwise draw two edges
    }
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

void Renderer::DrawFilledCircle(Point Centre, float RadiusLogical, Color InColor) {
    DrawArc(Centre, 0.0f, RadiusLogical, 0.0f, 360.0f, InColor);
}

void Renderer::DrawCircleOutline(Point Centre, float RadiusLogical, Color InColor, float ThicknessLogical) {
    if (ThicknessLogical <= 0.0f) {
        return;
    }
    DrawArc(Centre, std::max(0.0f, RadiusLogical - ThicknessLogical), RadiusLogical, 0.0f, 360.0f, InColor);
}

void Renderer::DrawArc(Point Centre, float InnerRadiusLogical, float OuterRadiusLogical, float StartDegrees,
                       float SweepDegrees, Color InColor) {
    const float Outer = OuterRadiusLogical * DpiScaleFactor;
    const float Inner = std::max(0.0f, InnerRadiusLogical * DpiScaleFactor);
    if (Outer <= 0.0f || Inner >= Outer || SweepDegrees == 0.0f) {
        return;
    }
    if (SweepDegrees < 0.0f) {
        StartDegrees += SweepDegrees;
        SweepDegrees = -SweepDegrees;
    }
    const bool  IsWholeRing = SweepDegrees >= 360.0f;
    const float Start = StartDegrees * Pi / 180.0f;
    const float Sweep = std::min(SweepDegrees, 360.0f) * Pi / 180.0f;

    const Point CentrePhysical = ToPhysicalPoint(Centre);
    const SDL_FPoint C{CentrePhysical.X, CentrePhysical.Y};
    const float Half = AaFeatherPhysical * 0.5f;

    // The arc is built from columns of vertices, each running outward from the centre at
    // one angle. A pie slice's column starts at the solid centre; a ring's starts with a
    // fade inside its inner edge. The fringe straddles each curved edge as in
    // DrawFilledRect, and a ring thinner than the feather keeps no solid core: its two
    // core radii meet in the middle.
    struct ColumnStop {
        float Radius;
        bool  IsSolid;
    };
    std::array<ColumnStop, 4> Stops{};
    int StopCount = 0;
    if (Inner <= 0.0f) {
        Stops = {{{0.0f, true}, {std::max(0.0f, Outer - Half), true}, {Outer + Half, false}}};
        StopCount = 3;
    } else {
        const float Middle = (Inner + Outer) * 0.5f;
        Stops = {{{std::max(0.0f, Inner - Half), false},
                  {std::min(Inner + Half, Middle), true},
                  {std::max(Outer - Half, Middle), true},
                  {Outer + Half, false}}};
        StopCount = 4;
    }

    // Same angular resolution as a rounded-rect corner of the outer radius, carried
    // across the sweep the way DrawRadialGradient carries it around a whole circle.
    const int Segments =
        std::max(1, static_cast<int>(std::ceil(4.0f * SegmentsForRadius(Outer) * Sweep / (2.0f * Pi))));
    const float SegmentSweep = Sweep / static_cast<float>(Segments);

    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    // Shift slides a column along the arc's direction of travel, which is how the two
    // straight ends get their fringe: each end's solid column moves in by Half and a
    // clear column sits Half outside it. Close to the centre a full Half could carry a
    // solid column past its neighbour, so there it moves at most half a segment.
    std::vector<SDL_Vertex> Vertices;
    Vertices.reserve(static_cast<std::size_t>(StopCount) * static_cast<std::size_t>(Segments + 3));
    auto PushColumn = [&](float Angle, float Shift, bool IsClearColumn) {
        const SDL_FPoint Outward{std::cos(Angle), std::sin(Angle)};
        const SDL_FPoint Along = Normal(Outward);
        for (int Index = 0; Index < StopCount; ++Index) {
            const ColumnStop& Stop = Stops[static_cast<std::size_t>(Index)];
            const float MaxShift = Stop.Radius * SegmentSweep * 0.5f;
            const float StopShift = IsClearColumn ? Shift : std::clamp(Shift, -MaxShift, MaxShift);
            Vertices.push_back({C + Outward * Stop.Radius + Along * StopShift,
                                (Stop.IsSolid && !IsClearColumn) ? Solid : Clear, {0.0f, 0.0f}});
        }
    };

    if (IsWholeRing) {
        for (int Step = 0; Step < Segments; ++Step) {
            PushColumn(Start + SegmentSweep * static_cast<float>(Step), 0.0f, false);
        }
    } else {
        PushColumn(Start, -Half, true);
        PushColumn(Start, Half, false);
        for (int Step = 1; Step < Segments; ++Step) {
            PushColumn(Start + SegmentSweep * static_cast<float>(Step), 0.0f, false);
        }
        PushColumn(Start + Sweep, -Half, false);
        PushColumn(Start + Sweep, Half, true);
    }

    const int ColumnCount = static_cast<int>(Vertices.size()) / StopCount;
    const int BandCount = IsWholeRing ? ColumnCount : ColumnCount - 1;
    std::vector<int> Indices;
    Indices.reserve(static_cast<std::size_t>(BandCount) * static_cast<std::size_t>(StopCount - 1) * 6);
    for (int Column = 0; Column < BandCount; ++Column) {
        const int A = Column * StopCount;
        const int B = ((Column + 1) % ColumnCount) * StopCount;
        for (int Stop = 0; Stop + 1 < StopCount; ++Stop) {
            Indices.insert(Indices.end(), {A + Stop, A + Stop + 1, B + Stop + 1, A + Stop, B + Stop + 1, B + Stop});
        }
    }

    DropDegenerateTriangles(Vertices, Indices);
    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawPolyline(std::span<const Point> Points, Color InColor, float ThicknessLogical,
                            LineJoin Join, LineCap Cap, bool Closed) {
    const float HalfWidth = ThicknessLogical * DpiScaleFactor * 0.5f;
    const std::vector<SDL_FPoint> Path = ToPhysicalPath(Points, Closed);
    if (HalfWidth <= 0.0f || Path.size() < 2) {
        return;
    }
    Closed = Closed && Path.size() > 2;

    const int PointCount = static_cast<int>(Path.size());
    const int SegmentCount = Closed ? PointCount : PointCount - 1;
    std::vector<SDL_FPoint> Directions(static_cast<std::size_t>(SegmentCount));
    std::vector<float> Lengths(static_cast<std::size_t>(SegmentCount));
    for (int Index = 0; Index < SegmentCount; ++Index) {
        const SDL_FPoint Delta = Path[static_cast<std::size_t>((Index + 1) % PointCount)] -
                                 Path[static_cast<std::size_t>(Index)];
        Lengths[static_cast<std::size_t>(Index)] = Length(Delta);
        Directions[static_cast<std::size_t>(Index)] = Delta * (1.0f / Length(Delta));
    }

    const float Half = AaFeatherPhysical * 0.5f;
    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    // The line is a strip of cross-sections, four vertices across: a fade and a solid
    // vertex on the side its segment's Normal points to, then a solid and a fade vertex
    // on the other. Solid vertices sit HalfWidth out from the path and fades Half beyond
    // them, as in DrawLine. Three bands of quads join each section to the one before.
    std::vector<SDL_Vertex> Vertices;
    std::vector<int> Indices;
    auto AddBands = [&](int A, int B) {
        for (int Lane = 0; Lane < 3; ++Lane) {
            Indices.insert(Indices.end(), {A + Lane, A + Lane + 1, B + Lane + 1, A + Lane, B + Lane + 1, B + Lane});
        }
    };
    auto PushSection = [&](SDL_FPoint At, SDL_FPoint NormalSide, SDL_FPoint OtherSide, bool IsClearSection = false) {
        const int Base = static_cast<int>(Vertices.size());
        const SDL_FColor Core = IsClearSection ? Clear : Solid;
        Vertices.push_back({At + NormalSide * (HalfWidth + Half), Clear, {0.0f, 0.0f}});
        Vertices.push_back({At + NormalSide * HalfWidth, Core, {0.0f, 0.0f}});
        Vertices.push_back({At + OtherSide * HalfWidth, Core, {0.0f, 0.0f}});
        Vertices.push_back({At + OtherSide * (HalfWidth + Half), Clear, {0.0f, 0.0f}});
        if (Base > 0) {
            AddBands(Base - 4, Base);
        }
    };

    // Butt and Square caps end in a clear section Half past the solid one, fading the
    // end the way the sides fade. A Round cap sweeps both sides of the section round to
    // meet at the tip, so its fringe runs round the curve. Outward points off the end.
    const int RoundSteps = 2 * SegmentsForRadius(HalfWidth);
    auto PushCap = [&](SDL_FPoint At, SDL_FPoint Outward, SDL_FPoint Across, bool IsStart) {
        if (Cap == LineCap::Round) {
            for (int Step = 0; Step <= RoundSteps; ++Step) {
                const float Angle = (Pi * 0.5f) * static_cast<float>(IsStart ? RoundSteps - Step : Step) /
                                    static_cast<float>(RoundSteps);
                const SDL_FPoint Ahead = Outward * std::sin(Angle);
                PushSection(At, Across * std::cos(Angle) + Ahead, -Across * std::cos(Angle) + Ahead);
            }
            return;
        }
        const float Extension = (Cap == LineCap::Square) ? HalfWidth : 0.0f;
        const SDL_FPoint SolidEnd = At + Outward * Extension;
        const SDL_FPoint FadeEnd = At + Outward * (Extension + Half);
        if (IsStart) {
            PushSection(FadeEnd, Across, -Across, true);
            PushSection(SolidEnd, Across, -Across);
        } else {
            PushSection(SolidEnd, Across, -Across);
            PushSection(FadeEnd, Across, -Across, true);
        }
    };

    auto PushJoin = [&](int Vertex, int InSegment, int OutSegment) {
        const SDL_FPoint At = Path[static_cast<std::size_t>(Vertex)];
        const SDL_FPoint InDirection = Directions[static_cast<std::size_t>(InSegment)];
        const SDL_FPoint OutDirection = Directions[static_cast<std::size_t>(OutSegment)];
        const SDL_FPoint InNormal = Normal(InDirection);
        const SDL_FPoint OutNormal = Normal(OutDirection);

        const SDL_FPoint Bisector = InNormal + OutNormal;
        const float BisectorLength = Length(Bisector);
        if (BisectorLength < 1e-4f) { // the line doubles straight back on itself
            PushSection(At, InNormal, -InNormal);
            PushSection(At, OutNormal, -OutNormal);
            return;
        }
        // Where the two edges on one side of the line meet, as an offset per unit of
        // half-width: 1 / cos(half the turn) along the bisector.
        const SDL_FPoint Miter = Bisector * (1.0f / BisectorLength);
        const float MiterScale = 1.0f / Dot(Miter, InNormal);
        const SDL_FPoint MiterOffset = Miter * MiterScale;

        // A positive turn bends towards the Normal side, so that side is the inside.
        const float Turn = Cross(InDirection, OutDirection);
        const float InsideSign = (Turn > 0.0f) ? 1.0f : -1.0f;

        // The inside of the turn shares the point where its two edges meet, unless that
        // point lies further back than either segment is long. Then each section keeps
        // its own inside edge and the inside of the corner overlaps itself slightly.
        const float InsideReach = (HalfWidth + Half) * std::abs(Dot(MiterOffset, InDirection));
        const bool InsideMeets = InsideReach <= std::min(Lengths[static_cast<std::size_t>(InSegment)],
                                                         Lengths[static_cast<std::size_t>(OutSegment)]);

        if (Join == LineJoin::Miter && MiterScale <= MiterLimit && InsideMeets) {
            PushSection(At, MiterOffset, -MiterOffset);
            return;
        }

        auto PushTurnSection = [&](SDL_FPoint Outside) {
            const SDL_FPoint Inside = InsideMeets ? MiterOffset * InsideSign : -Outside;
            if (InsideSign > 0.0f) {
                PushSection(At, Inside, Outside);
            } else {
                PushSection(At, Outside, Inside);
            }
        };
        const SDL_FPoint OutsideIn = InNormal * -InsideSign;
        if (Join == LineJoin::Round) {
            const float TurnAngle = std::atan2(std::abs(Turn), Dot(InDirection, OutDirection));
            const int Steps = std::max(
                1, static_cast<int>(std::ceil(TurnAngle / (Pi * 0.5f) * static_cast<float>(SegmentsForRadius(HalfWidth)))));
            for (int Step = 0; Step <= Steps; ++Step) {
                const float Angle = (Turn > 0.0f ? TurnAngle : -TurnAngle) * static_cast<float>(Step) /
                                    static_cast<float>(Steps);
                PushTurnSection(Rotated(OutsideIn, Angle));
            }
        } else {
            PushTurnSection(OutsideIn);
            PushTurnSection(OutNormal * -InsideSign);
        }
    };

    if (Closed) {
        for (int Index = 0; Index < PointCount; ++Index) {
            PushJoin(Index, (Index + SegmentCount - 1) % SegmentCount, Index);
        }
        AddBands(static_cast<int>(Vertices.size()) - 4, 0);
    } else {
        const SDL_FPoint FirstDirection = Directions.front();
        const SDL_FPoint LastDirection = Directions.back();
        PushCap(Path.front(), -FirstDirection, Normal(FirstDirection), true);
        for (int Index = 1; Index + 1 < PointCount; ++Index) {
            PushJoin(Index, Index - 1, Index);
        }
        PushCap(Path.back(), LastDirection, Normal(LastDirection), false);
    }

    DropDegenerateTriangles(Vertices, Indices);
    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
}

void Renderer::DrawFilledPolygon(std::span<const Point> Points, Color InColor) {
    const std::vector<SDL_FPoint> Path = ToPhysicalPath(Points, true);
    const int Count = static_cast<int>(Path.size());
    if (Count < 3) {
        return;
    }

    float TwiceArea = 0.0f;
    for (int Index = 0; Index < Count; ++Index) {
        TwiceArea += Cross(Path[static_cast<std::size_t>(Index)], Path[static_cast<std::size_t>((Index + 1) % Count)]);
    }
    if (std::abs(TwiceArea) < 1e-4f) {
        return;
    }
    // A positive area winds clockwise on screen, where each edge's Normal points inside.
    const float Winding = (TwiceArea > 0.0f) ? 1.0f : -1.0f;

    const float Half = AaFeatherPhysical * 0.5f;
    const SDL_FColor Solid = ToFColor(InColor);
    SDL_FColor Clear = Solid;
    Clear.a = 0.0f;

    // Each corner moves Half in (solid) and Half out (clear) along its miter, so the
    // fringe straddles the true edge as in DrawFilledRect. The interior is triangulated
    // over the inset corners, which keep the outline's shape.
    std::vector<SDL_Vertex> Vertices(static_cast<std::size_t>(2 * Count));
    for (int Index = 0; Index < Count; ++Index) {
        const SDL_FPoint Previous = Path[static_cast<std::size_t>((Index + Count - 1) % Count)];
        const SDL_FPoint Current = Path[static_cast<std::size_t>(Index)];
        const SDL_FPoint Next = Path[static_cast<std::size_t>((Index + 1) % Count)];
        const SDL_FPoint InOutward = Normal(Current - Previous) * (-Winding / Length(Current - Previous));
        const SDL_FPoint OutOutward = Normal(Next - Current) * (-Winding / Length(Next - Current));

        SDL_FPoint Offset = InOutward;
        const SDL_FPoint Bisector = InOutward + OutOutward;
        const float BisectorLength = Length(Bisector);
        if (BisectorLength >= 1e-4f) {
            const SDL_FPoint Miter = Bisector * (1.0f / BisectorLength);
            Offset = Miter * std::min(MiterLimit, 1.0f / Dot(Miter, InOutward));
        }
        Vertices[static_cast<std::size_t>(Index)] = {Current - Offset * Half, Solid, {0.0f, 0.0f}};
        Vertices[static_cast<std::size_t>(Count + Index)] = {Current + Offset * Half, Clear, {0.0f, 0.0f}};
    }

    std::vector<int> Indices = TriangulatePolygon(Path, Winding);
    Indices.reserve(Indices.size() + static_cast<std::size_t>(Count) * 6);
    for (int Index = 0; Index < Count; ++Index) {
        const int A = Index;
        const int B = (Index + 1) % Count;
        Indices.insert(Indices.end(), {A, B, Count + B, A, Count + B, Count + A});
    }

    DropDegenerateTriangles(Vertices, Indices);
    SDL_RenderGeometry(SdlRenderer, nullptr, Vertices.data(), static_cast<int>(Vertices.size()),
                       Indices.data(), static_cast<int>(Indices.size()));
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
    if (!ClipStack.empty() && ClipStack.back()) {
        // Intersect with the active clip so nested clips never exceed their parent.
        if (!SDL_GetRectIntersection(&*ClipStack.back(), &Requested, &Effective)) {
            Effective = {0, 0, 0, 0}; // disjoint → clip everything
        }
    }

    ClipStack.push_back(Effective);
    ApplyClipStack();
}

void Renderer::PushClipRectUnbounded(std::optional<Rect> RectLogical) {
    std::optional<SDL_Rect> Effective;
    if (RectLogical) {
        const SDL_FRect P = ToPhysical(*RectLogical);
        Effective         = SDL_Rect{static_cast<int>(std::lround(P.x)), static_cast<int>(std::lround(P.y)),
                                     static_cast<int>(std::lround(P.w)), static_cast<int>(std::lround(P.h))};
    }
    ClipStack.push_back(Effective);
    ApplyClipStack();
}

void Renderer::PopClipRect() {
    assert(!ClipStack.empty() && "PopClipRect called with an empty clip stack");
    ClipStack.pop_back();
    ApplyClipStack();
}

void Renderer::ApplyClipStack() {
    const bool Clipped = !ClipStack.empty() && ClipStack.back();
    SDL_SetRenderClipRect(SdlRenderer, Clipped ? &*ClipStack.back() : nullptr);
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

void Renderer::PushTransform(Rect SubjectRectLogical, const Transform& InTransform, float Opacity) {
    Opacity = std::clamp(Opacity, 0.0f, 1.0f);
    if (InTransform.IsIdentity() && Opacity >= 1.0f) {
        TransformStack.push_back({nullptr, nullptr, OriginOffsetLogical, SubjectRectLogical, InTransform, Opacity});
        return;
    }

    const int PhysicalWidth  = std::max(1, static_cast<int>(std::ceil(SubjectRectLogical.W * DpiScaleFactor)));
    const int PhysicalHeight = std::max(1, static_cast<int>(std::ceil(SubjectRectLogical.H * DpiScaleFactor)));

    SDL_Texture* CaptureTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_RGBA8888,
                                                     SDL_TEXTUREACCESS_TARGET, PhysicalWidth, PhysicalHeight);
    SDL_SetTextureBlendMode(CaptureTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(CaptureTexture, SDL_SCALEMODE_LINEAR);

    SDL_SetTextureAlphaModFloat(CaptureTexture, Opacity);

    TransformStack.push_back({CaptureTexture, SDL_GetRenderTarget(SdlRenderer), OriginOffsetLogical,
                              SubjectRectLogical, InTransform, Opacity});

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
    ApplyClipStack();

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
