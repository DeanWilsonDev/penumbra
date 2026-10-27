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
    Glyph{"cursor",
          [](const Pen& P) {
              P.Stroke({{5, 3}, {19, 11}, {13, 13}, {10, 20}, {5, 3}});
          }},
    Glyph{"move",
          [](const Pen& P) {
              P.Stroke({{12, 2}, {12, 22}});
              P.Stroke({{2, 12}, {22, 12}});
              P.Stroke({{9, 5}, {12, 2}, {15, 5}});
              P.Stroke({{9, 19}, {12, 22}, {15, 19}});
              P.Stroke({{5, 9}, {2, 12}, {5, 15}});
              P.Stroke({{19, 9}, {22, 12}, {19, 15}});
          }},
    Glyph{"rotate",
          [](const Pen& P) {
              P.Arc({12, 12}, 8, -60, 300);
              P.Stroke({{16, 2}, {16, 7}, {21, 7}});
          }},
    Glyph{"scale",
          [](const Pen& P) {
              P.Box({3, 9}, {15, 21}, 1);
              P.Stroke({{10, 14}, {21, 3}});
              P.Stroke({{15, 3}, {21, 3}, {21, 9}});
          }},
    Glyph{"grid",
          [](const Pen& P) {
              P.Box({3, 3}, {21, 21}, 1);
              P.Stroke({{9, 3}, {9, 21}});
              P.Stroke({{15, 3}, {15, 21}});
              P.Stroke({{3, 9}, {21, 9}});
              P.Stroke({{3, 15}, {21, 15}});
          }},
    Glyph{"magnet",
          [](const Pen& P) {
              P.Arc({12, 13}, 7, 180, 180);
              P.Stroke({{5, 13}, {5, 21}, {9, 21}, {9, 13}});
              P.Stroke({{15, 13}, {15, 21}, {19, 21}, {19, 13}});
          }},
    Glyph{"eye",
          [](const Pen& P) {
              P.Arc({12, 17}, 11, 220, 100);
              P.Arc({12, 7}, 11, 40, 100);
              P.Ring({12, 12}, 3);
          }},
    Glyph{"eye-off",
          [](const Pen& P) {
              P.Arc({12, 17}, 11, 220, 100);
              P.Arc({12, 7}, 11, 40, 100);
              P.Stroke({{4, 20}, {20, 4}});
          }},
    Glyph{"lock",
          [](const Pen& P) {
              P.Box({5, 11}, {19, 21}, 1);
              P.Arc({12, 11}, 4.5f, 180, 180);
              P.Dot({12, 16}, 0.5f);
          }},
    Glyph{"unlock",
          [](const Pen& P) {
              P.Box({5, 11}, {19, 21}, 1);
              P.Arc({16, 9}, 4.5f, 180, 150);
              P.Dot({12, 16}, 0.5f);
          }},
    Glyph{"camera",
          [](const Pen& P) {
              P.Stroke({{3, 8}, {8, 8}, {10, 5}, {14, 5}, {16, 8}, {21, 8}, {21, 20}, {3, 20}, {3, 8}});
              P.Ring({12, 14}, 3.5f);
          }},
    Glyph{"play", [](const Pen& P) { P.Stroke({{7, 4}, {19, 12}, {7, 20}, {7, 4}}); }},
    Glyph{"stop", [](const Pen& P) { P.Box({6, 6}, {18, 18}, 1); }},
    Glyph{"pause",
          [](const Pen& P) {
              P.Stroke({{8, 5}, {8, 19}});
              P.Stroke({{16, 5}, {16, 19}});
          }},
    Glyph{"undo",
          [](const Pen& P) {
              P.Arc({13, 13}, 7, -90, 270);
              P.Stroke({{3, 8}, {7, 12}, {11, 8}});
              P.Stroke({{6, 12}, {6, 13}});
          }},
    Glyph{"redo",
          [](const Pen& P) {
              P.Arc({11, 13}, 7, 0, 270);
              P.Stroke({{13, 8}, {17, 12}, {21, 8}});
              P.Stroke({{18, 12}, {18, 13}});
          }},
    Glyph{"save",
          [](const Pen& P) {
              P.Stroke({{3, 3}, {17, 3}, {21, 7}, {21, 21}, {3, 21}, {3, 3}});
              P.Stroke({{7, 3}, {7, 9}, {15, 9}, {15, 3}});
              P.Box({7, 14}, {17, 21}, 0);
          }},
    Glyph{"folder-open",
          [](const Pen& P) {
              P.Stroke({{3, 7}, {3, 4}, {11, 4}, {11, 7}});
              P.Stroke({{3, 7}, {3, 20}, {19, 20}, {23, 11}, {7, 11}, {3, 20}});
              P.Stroke({{3, 7}, {19, 7}, {19, 11}});
          }},
    Glyph{"search",
          [](const Pen& P) {
              P.Ring({10, 10}, 6);
              P.Stroke({{15, 15}, {21, 21}});
          }},
    Glyph{"gear",
          [](const Pen& P) {
              P.Ring({12, 12}, 3);
              P.Ring({12, 12}, 7);
              for (int Index = 0; Index < 8; ++Index) {
                  const float Angle = static_cast<float>(Index) * 0.7853982f;
                  const float C     = std::cos(Angle);
                  const float S     = std::sin(Angle);
                  P.Stroke({{12 + C * 8.0f, 12 + S * 8.0f}, {12 + C * 10.5f, 12 + S * 10.5f}});
              }
          }},
    Glyph{"trash",
          [](const Pen& P) {
              P.Stroke({{4, 6}, {20, 6}});
              P.Stroke({{9, 6}, {9, 3}, {15, 3}, {15, 6}});
              P.Stroke({{6, 6}, {7, 21}, {17, 21}, {18, 6}});
              P.Stroke({{10, 10}, {10, 17}});
              P.Stroke({{14, 10}, {14, 17}});
          }},
    Glyph{"copy",
          [](const Pen& P) {
              P.Box({8, 8}, {21, 21}, 1);
              P.Stroke({{5, 16}, {3, 16}, {3, 3}, {16, 3}, {16, 5}});
          }},
    Glyph{"cube",
          [](const Pen& P) {
              P.Stroke({{12, 2}, {21, 7}, {21, 17}, {12, 22}, {3, 17}, {3, 7}, {12, 2}});
              P.Stroke({{3, 7}, {12, 12}, {21, 7}});
              P.Stroke({{12, 12}, {12, 22}});
          }},
    Glyph{"sun",
          [](const Pen& P) {
              P.Ring({12, 12}, 4);
              for (int Index = 0; Index < 8; ++Index) {
                  const float Angle = static_cast<float>(Index) * 0.7853982f;
                  const float C     = std::cos(Angle);
                  const float S     = std::sin(Angle);
                  P.Stroke({{12 + C * 7.5f, 12 + S * 7.5f}, {12 + C * 10.5f, 12 + S * 10.5f}});
              }
          }},
    Glyph{"terminal",
          [](const Pen& P) {
              P.Box({2, 4}, {22, 20}, 1);
              P.Stroke({{6, 9}, {10, 12}, {6, 15}});
              P.Stroke({{12, 15}, {18, 15}});
          }},
    Glyph{"crosshair",
          [](const Pen& P) {
              P.Ring({12, 12}, 7);
              P.Stroke({{12, 2}, {12, 7}});
              P.Stroke({{12, 17}, {12, 22}});
              P.Stroke({{2, 12}, {7, 12}});
              P.Stroke({{17, 12}, {22, 12}});
          }},
    Glyph{"zoom-in",
          [](const Pen& P) {
              P.Ring({10, 10}, 6);
              P.Stroke({{15, 15}, {21, 21}});
              P.Stroke({{10, 7}, {10, 13}});
              P.Stroke({{7, 10}, {13, 10}});
          }},
    Glyph{"zoom-out",
          [](const Pen& P) {
              P.Ring({10, 10}, 6);
              P.Stroke({{15, 15}, {21, 21}});
              P.Stroke({{7, 10}, {13, 10}});
          }},
    Glyph{"info",
          [](const Pen& P) {
              P.Ring({12, 12}, 9);
              P.Stroke({{12, 11}, {12, 17}});
              P.Dot({12, 7.5f}, 0);
          }},
    Glyph{"filter", [](const Pen& P) { P.Stroke({{3, 4}, {21, 4}, {14, 13}, {14, 20}, {10, 20}, {10, 13}, {3, 4}}); }},
    Glyph{"pin",
          [](const Pen& P) {
              P.Stroke({{9, 3}, {15, 3}, {15, 10}, {19, 14}, {5, 14}, {9, 10}, {9, 3}});
              P.Stroke({{12, 14}, {12, 21}});
          }},
    Glyph{"refresh",
          [](const Pen& P) {
              P.Arc({12, 12}, 8, 200, 140);
              P.Arc({12, 12}, 8, 20, 140);
              P.Stroke({{16, 2}, {19.5f, 5.5f}, {15, 7}});
              P.Stroke({{8, 22}, {4.5f, 18.5f}, {9, 17}});
          }},
    Glyph{"image",
          [](const Pen& P) {
              P.Box({3, 4}, {21, 20}, 1);
              P.Stroke({{3, 17}, {9, 11}, {13, 15}, {16, 12}, {21, 17}});
              P.Dot({16, 8}, 0.7f);
          }},
    Glyph{"flag",
          [](const Pen& P) {
              P.Stroke({{5, 22}, {5, 3}, {19, 3}, {16, 8}, {19, 13}, {5, 13}});
          }},
    Glyph{"axis",
          [](const Pen& P) {
              P.Stroke({{4, 20}, {4, 4}});
              P.Stroke({{4, 20}, {20, 20}});
              P.Stroke({{4, 20}, {14, 10}});
              P.Stroke({{2, 6}, {4, 4}, {6, 6}});
              P.Stroke({{18, 18}, {20, 20}, {18, 22}});
          }},
    Glyph{"history",
          [](const Pen& P) {
              P.Arc({12, 12}, 9, -120, 330);
              P.Stroke({{2, 5}, {3, 10}, {8, 9}});
              P.Stroke({{12, 7}, {12, 12}, {16, 14}});
          }},
    Glyph{"command",
          [](const Pen& P) {
              P.Box({8, 8}, {16, 16}, 0);
              P.Ring({5, 5}, 2.5f);
              P.Ring({19, 5}, 2.5f);
              P.Ring({5, 19}, 2.5f);
              P.Ring({19, 19}, 2.5f);
          }},
    Glyph{"local-space",
          [](const Pen& P) {
              P.Stroke({{12, 2}, {21, 7}, {21, 17}, {12, 22}, {3, 17}, {3, 7}, {12, 2}});
              P.Stroke({{12, 12}, {12, 22}});
              P.Stroke({{12, 12}, {21, 7}});
              P.Stroke({{12, 12}, {3, 7}});
          }},
    Glyph{"world-space",
          [](const Pen& P) {
              P.Ring({12, 12}, 9);
              P.Stroke({{3, 12}, {21, 12}});
              P.Stroke({{12, 3}, {12, 21}});
              P.Arc({12, 12}, 4.5f, -90, 180);
              P.Arc({12, 12}, 4.5f, 90, 180);
          }},
    Glyph{"pivot",
          [](const Pen& P) {
              P.Ring({12, 12}, 8);
              P.Dot({12, 12}, 1.5f);
              P.Stroke({{12, 2}, {12, 6}});
              P.Stroke({{12, 18}, {12, 22}});
          }},
    Glyph{"collider",
          [](const Pen& P) {
              P.Box({4, 4}, {20, 20}, 1);
              P.Stroke({{4, 10}, {10, 4}});
              P.Stroke({{4, 16}, {16, 4}});
              P.Stroke({{8, 20}, {20, 8}});
              P.Stroke({{14, 20}, {20, 14}});
          }},
    Glyph{"wireframe",
          [](const Pen& P) {
              P.Stroke({{12, 2}, {21, 7}, {21, 17}, {12, 22}, {3, 17}, {3, 7}, {12, 2}});
              P.Stroke({{3, 7}, {12, 12}, {21, 7}});
              P.Stroke({{12, 12}, {12, 22}});
              P.Stroke({{3, 17}, {12, 12}, {21, 17}});
              P.Stroke({{12, 2}, {12, 12}});
          }},
    Glyph{"player",
          [](const Pen& P) {
              P.Ring({12, 6}, 3);
              P.Stroke({{6, 21}, {7, 13}, {17, 13}, {18, 21}});
          }},
    Glyph{"coin", [](const Pen& P) { P.Ring({12, 12}, 8); P.Ring({12, 12}, 4.5f); }},
    Glyph{"skull",
          [](const Pen& P) {
              P.Arc({12, 10}, 8, 180, 180);
              P.Stroke({{4, 10}, {4, 16}, {8, 18}, {8, 22}, {16, 22}, {16, 18}, {20, 16}, {20, 10}});
              P.Dot({9, 11}, 1.2f);
              P.Dot({15, 11}, 1.2f);
          }},
    Glyph{"platform",
          [](const Pen& P) {
              P.Box({3, 9}, {21, 15}, 1);
              P.Stroke({{6, 15}, {6, 21}});
              P.Stroke({{18, 15}, {18, 21}});
          }},
    Glyph{"checkpoint",
          [](const Pen& P) {
              P.Stroke({{6, 22}, {6, 3}});
              P.Stroke({{6, 3}, {18, 6}, {6, 10}});
              P.Stroke({{2, 22}, {10, 22}});
          }},
    Glyph{"trigger",
          [](const Pen& P) {
              P.Box({3, 3}, {21, 21}, 2);
              P.Stroke({{13, 6}, {8, 13}, {12, 13}, {11, 18}, {16, 11}, {12, 11}, {13, 6}});
          }},
    Glyph{"light",
          [](const Pen& P) {
              P.Arc({12, 10}, 6, 140, 260);
              P.Stroke({{8.5f, 15}, {9, 19}, {15, 19}, {15.5f, 15}});
              P.Stroke({{10, 22}, {14, 22}});
          }},
    Glyph{"spawn",
          [](const Pen& P) {
              P.Stroke({{12, 3}, {12, 14}});
              P.Stroke({{8, 10}, {12, 14}, {16, 10}});
              P.Arc({12, 14}, 8, 0, 180);
          }},
    Glyph{"dot-grid",
          [](const Pen& P) {
              for (const float X : {5.0f, 12.0f, 19.0f}) {
                  for (const float Y : {5.0f, 12.0f, 19.0f}) {
                      P.Dot({X, Y}, 0.7f);
                  }
              }
          }},
    Glyph{"arrow-up-right",
          [](const Pen& P) {
              P.Stroke({{5, 19}, {19, 5}});
              P.Stroke({{9, 5}, {19, 5}, {19, 15}});
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
