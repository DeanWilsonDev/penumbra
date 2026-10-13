#include "Penumbra/Widgets/BarChart.h"

#include <algorithm>
#include <cmath>

namespace Penumbra::Widgets {

namespace {

bool PointInRect(Point Point, Rect Rect) {
    return Point.X >= Rect.X && Point.X < Rect.X + Rect.W &&
           Point.Y >= Rect.Y && Point.Y < Rect.Y + Rect.H;
}

} // namespace

float BarChart::BandLength() const {
    const std::size_t Categories = CategoryCount();
    const float       Length     = Orientation == BarOrientation::Horizontal ? PlotRect.H : PlotRect.W;
    return Categories > 0 ? Length / static_cast<float>(Categories) : 0.0f;
}

float BarChart::BandStart(std::size_t Category) const {
    const float Origin = Orientation == BarOrientation::Horizontal ? PlotRect.Y : PlotRect.X;
    return Origin + BandLength() * static_cast<float>(Category);
}

void BarChart::DataExtent(float& Low, float& High) const {
    if (!Stacked) {
        AxisChartBase::DataExtent(Low, High);
        return;
    }
    Low  = 0.0f;
    High = 0.0f;
    for (std::size_t Category = 0; Category < CategoryCount(); ++Category) {
        float Up   = 0.0f;
        float Down = 0.0f;
        for (const ChartSeries& One : Series) {
            const float Value = Category < One.Values.size() ? One.Values[Category] : 0.0f;
            (Value >= 0.0f ? Up : Down) += Value;
        }
        High = std::max(High, Up);
        Low  = std::min(Low, Down);
    }
}

void BarChart::DrawChart(Render::Renderer& Renderer, Rect ContentRect, bool Snap, float DeltaSeconds,
                         float AppearProgress) {
    const ChartStyle& Look       = ChartVisualStyle;
    const bool        Horizontal = Orientation == BarOrientation::Horizontal;
    LayoutAxes(ContentRect, Horizontal, 0.0f, Snap, DeltaSeconds);
    DrawGrid(Renderer);

    const std::size_t Categories = CategoryCount();
    const float       Band       = BandLength();
    const Rect&       Plot       = PlotRect;
    auto BandRect = [&](std::size_t Category, float Offset, float Thickness, float From, float To) {
        const float Low  = std::min(From, To);
        const float Size = std::abs(To - From);
        const float At   = BandStart(Category) + Offset;
        return Horizontal ? Rect{Low, At, Size, Thickness} : Rect{At, Low, Thickness, Size};
    };

    const int Hovered = GetHoveredIndex();
    if (Hovered >= 0 && static_cast<std::size_t>(Hovered) < Categories && Look.ColorHover.A != 0) {
        const float Start = Horizontal ? Plot.X : Plot.Y + Plot.H;
        const float End   = Horizontal ? Plot.X + Plot.W : Plot.Y;
        Renderer.DrawFilledRect(BandRect(static_cast<std::size_t>(Hovered), 0.0f, Band, Start, End), Look.ColorHover);
    }

    const float Group     = Band * (1.0f - std::clamp(Look.BarGapFraction, 0.0f, 1.0f));
    const float GroupLead = (Band - Group) * 0.5f;
    const float Thickness = Stacked || Series.empty() ? Group : Group / static_cast<float>(Series.size());
    const float Base      = BaselineValue();

    // Bars stay in the plot, even while a shrinking range catches up with them.
    Renderer.PushClipRect(Plot);
    for (std::size_t Category = 0; Category < Categories; ++Category) {
        float Up   = 0.0f;
        float Down = 0.0f;
        for (std::size_t Index = 0; Index < Series.size(); ++Index) {
            const float Value = DisplayedValue(Index, Category);
            float       From  = Base;
            float       To    = Value;
            if (Stacked) {
                float& Cursor = Value >= 0.0f ? Up : Down;
                From          = Cursor;
                To            = Cursor + Value;
                Cursor        = To;
            }
            const float Offset = GroupLead + (Stacked ? 0.0f : Thickness * static_cast<float>(Index));
            const float Start  = ValuePosition(AppearValue(From, AppearProgress));
            const float End    = ValuePosition(AppearValue(To, AppearProgress));
            Renderer.DrawFilledRect(BandRect(Category, Offset, Thickness, Start, End), Series[Index].SeriesColor,
                                    Look.BarCornerRadiusLogical);
        }
    }
    Renderer.PopClipRect();

    DrawBaseline(Renderer);
    DrawCategoryLabels(Renderer, [&](std::size_t Category) { return BandStart(Category) + Band * 0.5f; }, Band);
}

int BarChart::HitTest(Point MouseLogical) const {
    const std::size_t Categories = CategoryCount();
    const float       Band       = BandLength();
    if (Categories == 0 || Band <= 0.0f || !PointInRect(MouseLogical, PlotRect)) {
        return -1;
    }
    const float Along = Orientation == BarOrientation::Horizontal ? MouseLogical.Y - PlotRect.Y
                                                                  : MouseLogical.X - PlotRect.X;
    const auto  Index = static_cast<std::size_t>(std::max(0.0f, std::floor(Along / Band)));
    return static_cast<int>(std::min(Index, Categories - 1));
}

} // namespace Penumbra::Widgets
