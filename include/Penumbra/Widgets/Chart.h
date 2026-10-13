#pragma once

#include "Penumbra/Anim/Animation.h"
#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Penumbra::Widgets {

// One slice of a DonutChart. Label is the app's, for a legend or tooltip; the chart
// doesn't draw it.
struct ChartSlice {
    std::string   Label;
    float         Value{0.0f};
    Render::Color SliceColor{0, 0, 0, 0};
};

// One series of a BarChart or LineChart: a value per category, in the chart's
// Labels order. Name is the app's, as ChartSlice::Label is.
struct ChartSeries {
    std::string        Name;
    std::vector<float> Values;
    Render::Color      SeriesColor{0, 0, 0, 0};
};

// What the chart widgets share. A chart is a Box: BoxStyle frames it and sizes it,
// and the chart draws in its content rect, clipped to it. Data and colours are plain
// fields; change them and the chart eases to the new values over
// ChartStyle::TransitionSeconds. It grows in over ChartStyle::AppearSeconds whenever
// it's shown: on the first frame it's drawn, and again after any frame it wasn't (it
// was hidden, or its subtree was). Animations run on the Renderer's frame clock (see
// Renderer::GetFrameIndex), so they advance whether or not input reaches the chart.
class ChartBase : public Box {
public:
    // The chart's own slots; ApplyStyle also copies the BoxStyle slice into Style.
    ChartStyle ChartVisualStyle{};
    void       ApplyStyle(const ChartStyle& StyleValue);

    // Labels are measured through FontBackend and drawn in Font. With no FontBackend
    // the chart draws no text.
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};

    // The content size the chart asks for when Style.Width/Height don't set one, as
    // CanvasWidget's.
    float PreferredWidthLogical{0.0f};
    float PreferredHeightLogical{0.0f};

    // The hovered category (BarChart, LineChart) or slice (DonutChart), or -1.
    // OnHoverChanged fires when it changes; a tooltip is the app's, through
    // OverlayHost. A chart with a hover callback or hover style takes the input while
    // the mouse is over it.
    int                      GetHoveredIndex() const { return HoveredIndex; }
    std::function<void(int)> OnHoverChanged;

    // Plays the appear animation again from the start on the next frame.
    void ReplayAppear() { ReplayRequested = true; }

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) final;

    // Brings the chart's data animations up to date and draws it. Snap is true when
    // the chart has just been shown (values jump to the data, and the appear
    // animation does the growing); otherwise the values ease toward the data, moved
    // on by DeltaSeconds. AppearProgress runs 0 to 1, already eased.
    virtual void DrawChart(Render::Renderer&, Rect ContentRect, bool Snap, float DeltaSeconds,
                           float AppearProgress) = 0;

    // The category or slice under MouseLogical, from the geometry of the last draw, or -1.
    virtual int HitTest(Point MouseLogical) const = 0;

    // Snaps Values to Target, or eases it there over TransitionSeconds.
    void SyncValues(Anim::TweenedValues& Values, std::span<const float> Target, bool Snap, float DeltaSeconds) const;

    float LineHeight() const; // the label font's line height, or 0 with no FontBackend
    float TextWidth(std::string_view Text) const;

    Rect LastContentRect{}; // where the chart was last drawn

private:
    bool HoverInUse() const;

    Anim::Tween   Appear;
    std::uint64_t LastDrawnFrame{0};
    bool          ReplayRequested{false};
    int           HoveredIndex{-1};
};

// A chart with a value axis and a category axis: BarChart and LineChart.
class AxisChartBase : public ChartBase {
public:
    std::vector<std::string> Labels; // one per category, along the category axis
    std::vector<ChartSeries> Series;

    // False draws only the data: no baseline, grid lines or labels, and the plot
    // fills the content rect. A LineChart without axes is a sparkline.
    bool ShowAxes{true};

    // The value axis's ends. Unset, they fit the data, rounded out to a whole tick.
    std::optional<float> MinValue;
    std::optional<float> MaxValue;

    // Formats a value-axis label. Unset, it prints just enough decimals for the step.
    std::function<std::string(float)> FormatValue;

protected:
    std::size_t CategoryCount() const;

    // The value of Series[SeriesIndex] at Category as drawn this frame: eased
    // between data sets, but before the appear animation. 0 past the end.
    float DisplayedValue(std::size_t SeriesIndex, std::size_t Category) const;

    // Grows Value in from the baseline, for the appear animation.
    float AppearValue(float Value, float AppearProgress) const;

    // The lowest and highest values the axis must reach for the current data.
    virtual void DataExtent(float& Low, float& High) const;

    // Whether the fitted axis always reaches 0. Bars stand on it; a line without axes
    // (a sparkline) spans just its own values.
    virtual bool AxisIncludesZero() const { return true; }

    // Lays out the plot in ContentRect and brings the value animations up to date.
    // InsetLogical keeps the data clear of the content edge (a LineChart's dots).
    void LayoutAxes(Rect ContentRect, bool ValueAxisIsHorizontal, float InsetLogical, bool Snap, float DeltaSeconds);

    // Where Value falls along the value axis, in logical pixels: a Y for a vertical
    // value axis, an X for a horizontal one.
    float ValuePosition(float Value) const;
    float BaselineValue() const; // 0 if the axis reaches it, else the nearer end

    void DrawGrid(Render::Renderer&) const;     // grid lines and value labels
    void DrawBaseline(Render::Renderer&) const; // the axis line, drawn over the data

    // Category labels: centred on CentreAt(Index) along the category axis, skipping
    // labels where they'd overlap; Spacing is the distance between categories.
    void DrawCategoryLabels(Render::Renderer&, const std::function<float(std::size_t)>& CentreAt,
                            float Spacing) const;

    Rect  PlotRect{};
    bool  ValueAxisHorizontal{false};
    float RangeLow{0.0f};
    float RangeHigh{1.0f};
    float TickStep{0.0f}; // 0: no ticks

private:
    std::string FormatTick(float Value) const;
    // Picks TickStep for a value axis PlotLength long, and rounds Low and High out to
    // whole steps unless MinValue or MaxValue pins them.
    void PlanTicks(float PlotLength, float& Low, float& High);

    std::vector<Anim::TweenedValues> SeriesValues;
    Anim::TweenedValues              Range;
};

} // namespace Penumbra::Widgets
