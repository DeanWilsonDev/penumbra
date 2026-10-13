#include "Penumbra/Widgets/LineChart.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Penumbra::Widgets {

namespace {

// Fills between Line and the horizontal line at BaselineY. Where the line crosses
// the baseline the area is cut into separate polygons there, since one polygon
// would cross itself.
void FillToBaseline(Render::Renderer& Renderer, const std::vector<Point>& Line, float BaselineY, Render::Color Fill) {
    std::vector<Point> Polygon;
    auto Flush = [&] {
        if (Polygon.size() >= 3) {
            Renderer.DrawFilledPolygon(Polygon, Fill);
        }
        Polygon.clear();
    };

    Polygon.push_back({Line.front().X, BaselineY});
    for (std::size_t Index = 0; Index < Line.size(); ++Index) {
        Polygon.push_back(Line[Index]);
        if (Index + 1 == Line.size()) {
            break;
        }
        const float Above = Line[Index].Y - BaselineY;
        const float Next  = Line[Index + 1].Y - BaselineY;
        if ((Above < 0.0f && Next > 0.0f) || (Above > 0.0f && Next < 0.0f)) {
            const float T = Above / (Above - Next);
            const Point Crossing{Line[Index].X + (Line[Index + 1].X - Line[Index].X) * T, BaselineY};
            Polygon.push_back(Crossing);
            Flush();
            Polygon.push_back(Crossing);
        }
    }
    Polygon.push_back({Line.back().X, BaselineY});
    Flush();
}

} // namespace

float LineChart::PointSpacing() const {
    const std::size_t Categories = CategoryCount();
    return Categories > 1 ? PlotRect.W / static_cast<float>(Categories - 1) : PlotRect.W;
}

float LineChart::PointX(std::size_t Category) const {
    return CategoryCount() > 1 ? PlotRect.X + PointSpacing() * static_cast<float>(Category)
                               : PlotRect.X + PlotRect.W * 0.5f;
}

void LineChart::DrawChart(Render::Renderer& Renderer, Rect ContentRect, bool Snap, float DeltaSeconds,
                          float AppearProgress) {
    const ChartStyle& Look = ChartVisualStyle;

    // Keep the line and its dots inside the content rect at the plot's edges.
    const float Inset =
        std::max({Look.LineThicknessLogical * 0.5f, Look.PointRadiusLogical, Look.HoverPointRadiusLogical, 0.0f});
    LayoutAxes(ContentRect, false, Inset, Snap, DeltaSeconds);
    DrawGrid(Renderer);

    const std::size_t Categories = CategoryCount();
    const Rect&       Plot       = PlotRect;
    const int         Hovered    = GetHoveredIndex();
    const bool HoverValid = Hovered >= 0 && static_cast<std::size_t>(Hovered) < Categories;

    if (HoverValid && Look.ColorHover.A != 0 && Look.GridLineThicknessLogical > 0.0f) {
        const float X = PointX(static_cast<std::size_t>(Hovered));
        Renderer.DrawLine({X, Plot.Y}, {X, Plot.Y + Plot.H}, Look.ColorHover, Look.GridLineThicknessLogical);
    }

    std::vector<std::vector<Point>> Lines(Series.size());
    for (std::size_t Index = 0; Index < Series.size(); ++Index) {
        Lines[Index].reserve(Categories);
        for (std::size_t Category = 0; Category < Categories; ++Category) {
            const float Value = AppearValue(DisplayedValue(Index, Category), AppearProgress);
            Lines[Index].push_back({PointX(Category), ValuePosition(Value)});
        }
    }

    const float BaselineY = ValuePosition(BaselineValue());
    if (Look.AreaOpacity > 0.0f && Categories > 1) {
        for (std::size_t Index = 0; Index < Series.size(); ++Index) {
            Render::Color Fill = Series[Index].SeriesColor;
            Fill.A = static_cast<std::uint8_t>(std::lround(Fill.A * std::clamp(Look.AreaOpacity, 0.0f, 1.0f)));
            FillToBaseline(Renderer, Lines[Index], BaselineY, Fill);
        }
    }

    DrawBaseline(Renderer);

    if (Look.LineThicknessLogical > 0.0f && Categories > 1) {
        for (std::size_t Index = 0; Index < Series.size(); ++Index) {
            Renderer.DrawPolyline(Lines[Index], Series[Index].SeriesColor, Look.LineThicknessLogical, Join, Cap);
        }
    }

    for (std::size_t Index = 0; Index < Series.size(); ++Index) {
        for (std::size_t Category = 0; Category < Categories; ++Category) {
            const bool  IsHovered = HoverValid && Category == static_cast<std::size_t>(Hovered);
            const float Radius =
                IsHovered && Look.HoverPointRadiusLogical > 0.0f ? Look.HoverPointRadiusLogical : Look.PointRadiusLogical;
            if (Radius > 0.0f) {
                Renderer.DrawFilledCircle(Lines[Index][Category], Radius, Series[Index].SeriesColor);
            }
        }
    }

    DrawCategoryLabels(Renderer, [&](std::size_t Category) { return PointX(Category); }, PointSpacing());
}

int LineChart::HitTest(Point MouseLogical) const {
    const std::size_t Categories = CategoryCount();
    const Rect&       Content    = LastContentRect;
    if (Categories == 0 || MouseLogical.X < Content.X || MouseLogical.X >= Content.X + Content.W ||
        MouseLogical.Y < Content.Y || MouseLogical.Y >= Content.Y + Content.H) {
        return -1;
    }
    if (Categories == 1) {
        return 0;
    }
    // The nearest category to the mouse along the X axis.
    const float Nearest = std::round((MouseLogical.X - PlotRect.X) / PointSpacing());
    return static_cast<int>(std::clamp(Nearest, 0.0f, static_cast<float>(Categories - 1)));
}

} // namespace Penumbra::Widgets
