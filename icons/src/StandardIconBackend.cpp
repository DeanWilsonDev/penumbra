#include "PenumbraIcons/StandardIconBackend.h"

#include "Penumbra/Render/Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <span>

namespace PenumbraIcons {

namespace {

using Penumbra::Point;
using Penumbra::Rect;
using Penumbra::Render::Color;
using Penumbra::Render::Renderer;

constexpr float GridSize = 24.0f;

class Pen {
public:
    Pen(Renderer& InTarget, Rect Bounds, Color InTint, float InThickness)
        : Target(InTarget), Tint(InTint), Thickness(InThickness) {
        const float Side = std::min(Bounds.W, Bounds.H);
        Origin = {Bounds.X + (Bounds.W - Side) * 0.5f, Bounds.Y + (Bounds.H - Side) * 0.5f};
        Scale  = Side / GridSize;
    }

    void Stroke(std::initializer_list<Point> GridPoints) const {
        const Point* Previous = nullptr;
        for (const Point& GridPoint : GridPoints) {
            if (Previous != nullptr) {
                Target.DrawLine(At(*Previous), At(GridPoint), Tint, Thickness);
            }
            Previous = &GridPoint;
        }
        for (const Point& GridPoint : GridPoints) {
            Disc(At(GridPoint), Thickness * 0.5f);
        }
    }

    void Dot(Point GridCenter, float GridRadius) const { Disc(At(GridCenter), GridRadius * Scale + Thickness * 0.5f); }

    void Ring(Point GridCenter, float GridRadius) const {
        const Point Center = At(GridCenter);
        const float Radius = GridRadius * Scale + Thickness * 0.5f;
        Target.DrawRectOutline({Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f}, Tint, Thickness,
                               Radius);
    }

    void Box(Point GridMin, Point GridMax, float GridCornerRadius) const {
        const Point Min  = At(GridMin);
        const Point Max  = At(GridMax);
        const float Half = Thickness * 0.5f;
        Target.DrawRectOutline({Min.X - Half, Min.Y - Half, Max.X - Min.X + Thickness, Max.Y - Min.Y + Thickness}, Tint,
                               Thickness, GridCornerRadius * Scale + Half);
    }

    // A stroked arc, angles as in Renderer::DrawArc: clockwise from 3 o'clock.
    void Arc(Point GridCenter, float GridRadius, float StartDegrees, float SweepDegrees) const {
        const float Radius = GridRadius * Scale;
        const float Half   = Thickness * 0.5f;
        Target.DrawArc(At(GridCenter), Radius - Half, Radius + Half, StartDegrees, SweepDegrees, Tint);
    }

    void Fill(std::span<const Point> GridPoints) const {
        for (std::size_t Index = 2; Index < GridPoints.size(); ++Index) {
            Target.DrawTriangleFilled(At(GridPoints[0]), At(GridPoints[Index - 1]), At(GridPoints[Index]), Tint);
        }
    }

private:
    Point At(Point GridPoint) const { return {Origin.X + GridPoint.X * Scale, Origin.Y + GridPoint.Y * Scale}; }

    void Disc(Point Center, float Radius) const {
        Target.DrawFilledRect({Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f}, Tint, Radius);
    }

    Renderer& Target;
    Color     Tint;
    float     Thickness;
    Point     Origin{};
    float     Scale{1.0f};
};

struct Glyph {
    std::string_view Name;
    void (*Draw)(const Pen&);
};

constexpr std::array Glyphs{
    Glyph{"close",
          [](const Pen& P) {
              P.Stroke({{6, 6}, {18, 18}});
              P.Stroke({{18, 6}, {6, 18}});
          }},
    Glyph{"chevron-left", [](const Pen& P) { P.Stroke({{15, 6}, {9, 12}, {15, 18}}); }},
    Glyph{"chevron-right", [](const Pen& P) { P.Stroke({{9, 6}, {15, 12}, {9, 18}}); }},
    Glyph{"chevron-up", [](const Pen& P) { P.Stroke({{6, 15}, {12, 9}, {18, 15}}); }},
    Glyph{"chevron-down", [](const Pen& P) { P.Stroke({{6, 9}, {12, 15}, {18, 9}}); }},
    Glyph{"chevrons-up",
          [](const Pen& P) {
              P.Stroke({{6, 12}, {12, 6}, {18, 12}});
              P.Stroke({{6, 18}, {12, 12}, {18, 18}});
          }},
    Glyph{"chevrons-down",
          [](const Pen& P) {
              P.Stroke({{6, 6}, {12, 12}, {18, 6}});
              P.Stroke({{6, 12}, {12, 18}, {18, 12}});
          }},
    Glyph{"hamburger",
          [](const Pen& P) {
              P.Stroke({{4, 6}, {20, 6}});
              P.Stroke({{4, 12}, {20, 12}});
              P.Stroke({{4, 18}, {20, 18}});
          }},
    Glyph{"kebab",
          [](const Pen& P) {
              P.Dot({12, 5}, 1.0f);
              P.Dot({12, 12}, 1.0f);
              P.Dot({12, 19}, 1.0f);
          }},
    Glyph{"plus",
          [](const Pen& P) {
              P.Stroke({{12, 5}, {12, 19}});
              P.Stroke({{5, 12}, {19, 12}});
          }},
    Glyph{"minus", [](const Pen& P) { P.Stroke({{5, 12}, {19, 12}}); }},
    Glyph{"equals",
          [](const Pen& P) {
              P.Stroke({{5, 9}, {19, 9}});
              P.Stroke({{5, 15}, {19, 15}});
          }},
    Glyph{"check", [](const Pen& P) { P.Stroke({{4, 12}, {9, 17}, {20, 6}}); }},
    Glyph{"document",
          [](const Pen& P) {
              P.Stroke({{5, 3}, {14, 3}, {19, 8}, {19, 21}, {5, 21}, {5, 3}});
              P.Stroke({{14, 3}, {14, 8}, {19, 8}});
          }},
    Glyph{"folder",
          [](const Pen& P) {
              P.Stroke({{3, 7}, {3, 4}, {11, 4}, {11, 7}});
              P.Stroke({{3, 7}, {21, 7}, {21, 20}, {3, 20}, {3, 7}});
          }},
    Glyph{"tag",
          [](const Pen& P) {
              P.Stroke({{3, 6}, {15, 6}, {21, 12}, {15, 18}, {3, 18}, {3, 6}});
              P.Dot({7, 12}, 0.5f);
          }},
    Glyph{"list",
          [](const Pen& P) {
              for (const float Y : {6.0f, 12.0f, 18.0f}) {
                  P.Dot({4, Y}, 0.5f);
                  P.Stroke({{8, Y}, {20, Y}});
              }
          }},
    Glyph{"pencil",
          [](const Pen& P) {
              P.Stroke({{15, 4}, {20, 9}, {8, 21}, {3, 21}, {3, 16}, {15, 4}});
              P.Stroke({{12, 7}, {17, 12}});
          }},
    Glyph{"home",
          [](const Pen& P) {
              P.Stroke({{3, 10}, {12, 3}, {21, 10}});
              P.Stroke({{5, 8.5f}, {5, 21}, {19, 21}, {19, 8.5f}});
              P.Stroke({{10, 21}, {10, 15}, {14, 15}, {14, 21}});
          }},
    Glyph{"warning",
          [](const Pen& P) {
              P.Stroke({{12, 2}, {23, 21}, {1, 21}, {12, 2}});
              P.Stroke({{12, 8.5f}, {12, 12.5f}});
              P.Dot({12, 16.5f}, 0);
          }},
    Glyph{"circle", [](const Pen& P) { P.Ring({12, 12}, 9); }},
    Glyph{"circle-half",
          [](const Pen& P) {
              P.Ring({12, 12}, 9);
              std::array<Point, 13> RightHalf{};
              for (std::size_t Index = 0; Index < RightHalf.size(); ++Index) {
                  const float Angle =
                      std::numbers::pi_v<float> * (static_cast<float>(Index) / (RightHalf.size() - 1) - 0.5f);
                  RightHalf[Index] = {12 + 9 * std::cos(Angle), 12 + 9 * std::sin(Angle)};
              }
              P.Fill(RightHalf);
              P.Stroke({{12, 3}, {12, 21}});
          }},
    Glyph{"circle-check",
          [](const Pen& P) {
              P.Ring({12, 12}, 9);
              P.Stroke({{8, 12.5f}, {11, 15.5f}, {16.5f, 9.5f}});
          }},
    Glyph{"bolt", [](const Pen& P) { P.Stroke({{13, 2}, {4, 14}, {12, 14}, {11, 22}, {20, 10}, {12, 10}, {13, 2}}); }},
    Glyph{"layers",
          [](const Pen& P) {
              P.Stroke({{12, 3}, {22, 8.5f}, {12, 14}, {2, 8.5f}, {12, 3}});
              P.Stroke({{2, 14.5f}, {12, 20}, {22, 14.5f}});
          }},
    Glyph{"bookmark", [](const Pen& P) { P.Stroke({{6, 3}, {18, 3}, {18, 21}, {12, 16}, {6, 21}, {6, 3}}); }},
    Glyph{"square-check",
          [](const Pen& P) {
              P.Box({4, 4}, {20, 20}, 3);
              P.Stroke({{8, 12}, {11, 15}, {16, 9}});
          }},
    Glyph{"kanban",
          [](const Pen& P) {
              P.Box({3, 3}, {21, 21}, 3);
              P.Stroke({{8, 7}, {8, 15}});
              P.Stroke({{12, 7}, {12, 11}});
              P.Stroke({{16, 7}, {16, 17}});
          }},
    Glyph{"chart-bar",
          [](const Pen& P) {
              P.Stroke({{3, 3}, {3, 21}, {21, 21}});
              P.Stroke({{8, 17}, {8, 13}});
              P.Stroke({{13, 17}, {13, 6}});
              P.Stroke({{18, 17}, {18, 10}});
          }},
    Glyph{"chart-line",
          [](const Pen& P) {
              P.Stroke({{3, 3}, {3, 21}, {21, 21}});
              P.Stroke({{7, 16}, {11, 11}, {14, 14}, {20, 7}});
          }},
    Glyph{"chart-pie",
          [](const Pen& P) {
              P.Arc({10, 14}, 8, 0, 270);
              P.Stroke({{10, 6}, {10, 14}, {18, 14}});
              P.Arc({14, 10}, 8, -90, 90);
              P.Stroke({{14, 2}, {14, 10}, {22, 10}});
          }},
};

} // namespace

StandardIconBackend::StandardIconBackend(float InStrokeThicknessLogical, Penumbra::Render::Color InDefaultColor)
    : StrokeThicknessLogical(InStrokeThicknessLogical), DefaultColor(InDefaultColor) {}

void StandardIconBackend::DrawIcon(Renderer& Renderer, std::string_view IconName, Rect BoundsLogical,
                                   Color IconColor) {
    const auto Found = std::ranges::find(Glyphs, IconName, &Glyph::Name);
    if (Found == Glyphs.end()) {
        return;
    }
    Found->Draw(Pen(Renderer, BoundsLogical, IconColor.A != 0 ? IconColor : DefaultColor, StrokeThicknessLogical));
}

std::span<const std::string_view> StandardIconBackend::Names() {
    static const auto All = [] {
        std::array<std::string_view, Glyphs.size()> Result{};
        std::ranges::transform(Glyphs, Result.begin(), &Glyph::Name);
        return Result;
    }();
    return All;
}

} // namespace PenumbraIcons
