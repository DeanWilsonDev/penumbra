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

SDL_FColor ToFColor(SDL_Color Color) {
    return {Color.r / 255.0f, Color.g / 255.0f, Color.b / 255.0f, Color.a / 255.0f};
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

void Renderer::DrawFilledRect(SDL_FRect RectLogical, SDL_Color Color, float CornerRadiusLogical) {
    const SDL_FRect R = ToPhysical(RectLogical);
    float Radius = CornerRadiusLogical * DpiScaleFactor;

    if (Radius <= 0.0f) {
        SDL_SetRenderDrawColor(SdlRenderer, Color.r, Color.g, Color.b, Color.a);
        SDL_RenderFillRect(SdlRenderer, &R);
        return;
    }

    Radius = std::min(Radius, std::min(R.w, R.h) * 0.5f);
    const int Segments = SegmentsForRadius(Radius);
    const RoundedRing Ring = BuildRoundedRing(R, Radius, Segments);
    const int Count = static_cast<int>(Ring.Points.size());
    const float Half = AaFeatherPhysical * 0.5f;

    const SDL_FColor Solid = ToFColor(Color);
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

void Renderer::DrawRectOutline(SDL_FRect RectLogical, SDL_Color Color, float ThicknessLogical,
                               float CornerRadiusLogical) {
    const SDL_FRect R = ToPhysical(RectLogical);
    const float T = ThicknessLogical * DpiScaleFactor;
    float Radius = CornerRadiusLogical * DpiScaleFactor;

    if (Radius <= 0.0f) {
        SDL_SetRenderDrawColor(SdlRenderer, Color.r, Color.g, Color.b, Color.a);
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
        DrawFilledRect(RectLogical, Color, CornerRadiusLogical);
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

    const SDL_FColor Solid = ToFColor(Color);
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

SDL_Renderer* Renderer::GetSdlRenderer() const {
    return SdlRenderer;
}

void Renderer::DrawTexture(SDL_Texture* Texture, SDL_FRect DestLogical) {
    if (!Texture) {
        return;
    }
    // The scene texture is sized in physical pixels (content × dpi); the logical
    // destination scales to the same physical extent, so the blit is 1:1 — no resample.
    const SDL_FRect Destination = ToPhysical(DestLogical);
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
