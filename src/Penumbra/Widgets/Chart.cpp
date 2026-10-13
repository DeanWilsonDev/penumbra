#include "Penumbra/Widgets/Chart.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Penumbra::Widgets {

namespace {

// Slack for comparing tick arithmetic that floats can't do exactly.
constexpr float TickTolerance = 1e-4f;

// The smallest 1, 2 or 5 x 10^n at or above RawStep.
float NiceStepAtLeast(float RawStep) {
    if (!(RawStep > 0.0f) || !std::isfinite(RawStep)) {
        return 1.0f;
    }
    const float Magnitude = std::pow(10.0f, std::floor(std::log10(RawStep)));
    const float Fraction  = RawStep / Magnitude;
    float       Nice      = 10.0f;
    if (Fraction <= 1.0f + TickTolerance) {
        Nice = 1.0f;
    } else if (Fraction <= 2.0f + TickTolerance) {
        Nice = 2.0f;
    } else if (Fraction <= 5.0f + TickTolerance) {
        Nice = 5.0f;
    }
    return Nice * Magnitude;
}

// Calls Visit with every multiple of Step from Low to High, inclusive.
template <typename Visitor> void ForEachTick(float Low, float High, float Step, Visitor&& Visit) {
    if (!(Step > 0.0f)) {
        return;
    }
    constexpr int MaxTicks = 1000; // a guard against a step far too small for its range
    const float   First    = std::ceil(Low / Step - TickTolerance);
    for (int Index = 0; Index < MaxTicks; ++Index) {
        const float Tick = (First + static_cast<float>(Index)) * Step;
        if (Tick > High + Step * TickTolerance) {
            break;
        }
        Visit(Tick);
    }
}

Rect Inset(Rect Value, float Amount) {
    return {Value.X + Amount, Value.Y + Amount, std::max(0.0f, Value.W - 2.0f * Amount),
            std::max(0.0f, Value.H - 2.0f * Amount)};
}

} // namespace

// ---------------------------------------------------------------------------
// ChartBase
// ---------------------------------------------------------------------------

void ChartBase::ApplyStyle(const ChartStyle& StyleValue) {
    ChartVisualStyle = StyleValue;
    Style            = static_cast<const BoxStyle&>(StyleValue);
}

Point ChartBase::MeasureContent(Point /*AvailableContentSize*/) {
    return {PreferredWidthLogical, PreferredHeightLogical};
}

bool ChartBase::HoverInUse() const {
    return OnHoverChanged || ChartVisualStyle.ColorHover.A != 0 || ChartVisualStyle.HoverPointRadiusLogical > 0.0f;
}

bool ChartBase::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Over       = IsEnabled && HoverInUse() && Input.IsPointerOver(ArrangedRect);
    const int  NewHovered = Over ? HitTest(Input.MousePosition) : -1;
    if (NewHovered != HoveredIndex) {
        HoveredIndex = NewHovered;
        if (OnHoverChanged) {
            OnHoverChanged(HoveredIndex);
        }
    }

    const bool BoxConsumed = Box::UpdateInteractionState(Input);
    return BoxConsumed || Over;
}

void ChartBase::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (ContentRect.W <= 0.0f || ContentRect.H <= 0.0f) {
        return;
    }

    const std::uint64_t Frame = Renderer.GetFrameIndex();
    bool                Snap  = false;
    float               Delta = 0.0f;
    if (Frame == 0) {
        // No frame clock (BeginFrame never ran), so nothing could animate: draw the
        // data as it stands.
        Snap   = true;
        Appear = {};
    } else if (LastDrawnFrame == 0 || ReplayRequested || (Frame != LastDrawnFrame && Frame != LastDrawnFrame + 1)) {
        // First drawn, shown again after a frame it wasn't drawn in, or asked to replay.
        Snap = true;
        Appear.Start(ChartVisualStyle.AppearSeconds);
        ReplayRequested = false;
    } else if (Frame == LastDrawnFrame + 1) {
        Delta = Renderer.GetFrameDeltaSeconds();
    } // else drawn twice in one frame, and time hasn't moved
    LastDrawnFrame = Frame;
    Appear.Update(Delta);

    LastContentRect = ContentRect;
    Renderer.PushClipRect(ContentRect);
    DrawChart(Renderer, ContentRect, Snap, Delta, Anim::EaseOutCubic(Appear.Progress()));
    Renderer.PopClipRect();
}

void ChartBase::SyncValues(Anim::TweenedValues& Values, std::span<const float> Target, bool Snap,
                           float DeltaSeconds) const {
    if (Snap) {
        Values.SnapTo(Target);
        return;
    }
    Values.RetargetTo(Target, ChartVisualStyle.TransitionSeconds);
    Values.Update(DeltaSeconds);
}

float ChartBase::LineHeight() const {
    return FontBackend ? FontBackend->MeasureText(Font, "Ag").HeightLogical : 0.0f;
}

float ChartBase::TextWidth(std::string_view Text) const {
    return FontBackend && !Text.empty() ? FontBackend->MeasureTextWidth(Font, Text) : 0.0f;
}

// ---------------------------------------------------------------------------
// AxisChartBase
// ---------------------------------------------------------------------------

std::size_t AxisChartBase::CategoryCount() const {
    std::size_t Count = Labels.size();
    for (const ChartSeries& One : Series) {
        Count = std::max(Count, One.Values.size());
    }
    return Count;
}

float AxisChartBase::DisplayedValue(std::size_t SeriesIndex, std::size_t Category) const {
    if (SeriesIndex >= SeriesValues.size() || Category >= SeriesValues[SeriesIndex].Size()) {
        return 0.0f;
    }
    return SeriesValues[SeriesIndex].ValueAt(Category);
}

float AxisChartBase::AppearValue(float Value, float AppearProgress) const {
    const float Base = BaselineValue();
    return Base + (Value - Base) * AppearProgress;
}

void AxisChartBase::DataExtent(float& Low, float& High) const {
    bool Any = false;
    for (const ChartSeries& One : Series) {
        for (const float Value : One.Values) {
            Low  = Any ? std::min(Low, Value) : Value;
            High = Any ? std::max(High, Value) : Value;
            Any  = true;
        }
    }
    if (!Any) {
        Low  = 0.0f;
        High = 0.0f;
    }
}

float AxisChartBase::BaselineValue() const { return std::clamp(0.0f, RangeLow, RangeHigh); }

float AxisChartBase::ValuePosition(float Value) const {
    const float Span = RangeHigh - RangeLow;
    const float T    = Span > 0.0f ? (Value - RangeLow) / Span : 0.0f;
    return ValueAxisHorizontal ? PlotRect.X + PlotRect.W * T : PlotRect.Y + PlotRect.H * (1.0f - T);
}

std::string AxisChartBase::FormatTick(float Value) const {
    if (std::abs(Value) < TickStep * TickTolerance) {
        Value = 0.0f; // no "-0"
    }
    if (FormatValue) {
        return FormatValue(Value);
    }
    const int Decimals =
        TickStep >= 1.0f || TickStep <= 0.0f ? 0 : static_cast<int>(std::ceil(-std::log10(TickStep) - TickTolerance));
    return std::format("{:.{}f}", Value, Decimals);
}

void AxisChartBase::PlanTicks(float PlotLength, float& Low, float& High) {
    const float MinSpacing = ChartVisualStyle.TickSpacingLogical;
    const float Gap        = ChartVisualStyle.LabelGapLogical;
    const bool  Labelled   = ValueAxisHorizontal && LineHeight() > 0.0f;

    const float MaxIntervals = std::max(1.0f, std::floor(PlotLength / MinSpacing));
    float       Step         = NiceStepAtLeast((High - Low) / MaxIntervals);

    // Widen the step until the ticks are far enough apart, and on a horizontal value
    // axis until their labels clear each other too.
    constexpr int MaxAttempts = 8;
    for (int Attempt = 0;; ++Attempt) {
        TickStep               = Step;
        const float NiceLow    = MinValue ? Low : std::floor(Low / Step + TickTolerance) * Step;
        const float NiceHigh   = MaxValue ? High : std::ceil(High / Step - TickTolerance) * Step;
        const float Intervals  = (NiceHigh - NiceLow) / Step;
        const float SpacingNow = Intervals > 0.0f ? PlotLength / Intervals : PlotLength;

        float Needed = MinSpacing;
        if (Labelled) {
            float Widest = 0.0f;
            ForEachTick(NiceLow, NiceHigh, Step, [&](float Tick) { Widest = std::max(Widest, TextWidth(FormatTick(Tick))); });
            Needed = std::max(Needed, Widest + Gap);
        }

        if (SpacingNow + TickTolerance >= Needed || Attempt + 1 == MaxAttempts) {
            Low  = NiceLow;
            High = NiceHigh;
            return;
        }
        Step = NiceStepAtLeast(Step * 1.01f); // the next step up: 1 -> 2 -> 5 -> 10
    }
}

void AxisChartBase::LayoutAxes(Rect ContentRect, bool ValueAxisIsHorizontal, float InsetLogical, bool Snap,
                               float DeltaSeconds) {
    ValueAxisHorizontal = ValueAxisIsHorizontal;
    const std::size_t Categories = CategoryCount();

    // Ease each series toward its data, padded out to every category.
    SeriesValues.resize(Series.size());
    std::vector<float> Target;
    for (std::size_t Index = 0; Index < Series.size(); ++Index) {
        Target.assign(Categories, 0.0f);
        std::ranges::copy(Series[Index].Values, Target.begin());
        SyncValues(SeriesValues[Index], Target, Snap, DeltaSeconds);
    }

    float Low  = 0.0f;
    float High = 0.0f;
    DataExtent(Low, High);
    if (AxisIncludesZero()) {
        Low  = std::min(Low, 0.0f);
        High = std::max(High, 0.0f);
    }
    Low  = MinValue.value_or(Low);
    High = MaxValue.value_or(High);
    if (!(High > Low)) {
        High = Low + 1.0f; // flat or empty data still gets an axis
    }

    const ChartStyle& Look           = ChartVisualStyle;
    const float       LineH          = ShowAxes ? LineHeight() : 0.0f;
    const bool        Ticks          = ShowAxes && Look.TickSpacingLogical > 0.0f;
    const bool        ValueLabels    = Ticks && LineH > 0.0f;
    const bool        CategoryLabels = LineH > 0.0f && !Labels.empty();
    const float       Gap            = Look.LabelGapLogical;

    TickStep  = 0.0f;
    Rect Plot = ContentRect;
    if (!ValueAxisHorizontal) {
        // Values up the left, categories along the bottom. Half a line above the plot
        // leaves room for the top value label, which is centred on its grid line.
        const float Top    = ValueLabels ? LineH * 0.5f : 0.0f;
        const float Bottom = CategoryLabels ? Gap + LineH : 0.0f;
        Plot.Y += Top;
        Plot.H = std::max(0.0f, Plot.H - Top - Bottom);
        Plot   = Inset(Plot, InsetLogical);
        if (Ticks) {
            PlanTicks(Plot.H, Low, High);
        }
        if (ValueLabels) {
            float Widest = 0.0f;
            ForEachTick(Low, High, TickStep, [&](float Tick) { Widest = std::max(Widest, TextWidth(FormatTick(Tick))); });
            const float Gutter = Widest + Gap;
            Plot.X += Gutter;
            Plot.W = std::max(0.0f, Plot.W - Gutter);
        }
    } else {
        // Categories down the left, values along the bottom.
        float Gutter = 0.0f;
        if (CategoryLabels) {
            float Widest = 0.0f;
            for (const std::string& Label : Labels) {
                Widest = std::max(Widest, TextWidth(Label));
            }
            Gutter = Widest + Gap;
        }
        const float Bottom = ValueLabels ? Gap + LineH : 0.0f;
        Plot.X += Gutter;
        Plot.W = std::max(0.0f, Plot.W - Gutter);
        Plot.H = std::max(0.0f, Plot.H - Bottom);
        Plot   = Inset(Plot, InsetLogical);
        if (Ticks) {
            PlanTicks(Plot.W, Low, High);
        }
        if (ValueLabels) {
            // The last value label is centred on the plot's right edge; keep it inside.
            float Widest = 0.0f;
            ForEachTick(Low, High, TickStep, [&](float Tick) { Widest = std::max(Widest, TextWidth(FormatTick(Tick))); });
            Plot.W = std::max(0.0f, Plot.W - Widest * 0.5f);
        }
    }
    PlotRect = Plot;

    // The range eases too, so the bars and the grid move together when the data does.
    const float RangeTarget[] = {Low, High};
    SyncValues(Range, RangeTarget, Snap, DeltaSeconds);
    RangeLow  = Range.ValueAt(0);
    RangeHigh = std::max(Range.ValueAt(1), RangeLow + TickTolerance);
}

void AxisChartBase::DrawGrid(Render::Renderer& Renderer) const {
    if (!ShowAxes || TickStep <= 0.0f) {
        return;
    }
    const ChartStyle& Look      = ChartVisualStyle;
    const bool        Lines     = Look.GridLineThicknessLogical > 0.0f && Look.ColorGridLine.A != 0;
    const float       LineH     = LineHeight();
    const bool        Labelled  = LineH > 0.0f && Look.ColorLabel.A != 0;
    const float       Gap       = Look.LabelGapLogical;
    const Rect&       Plot      = PlotRect;

    ForEachTick(RangeLow, RangeHigh, TickStep, [&](float Tick) {
        const float At = ValuePosition(Tick);
        if (Lines) {
            if (ValueAxisHorizontal) {
                Renderer.DrawLine({At, Plot.Y}, {At, Plot.Y + Plot.H}, Look.ColorGridLine, Look.GridLineThicknessLogical);
            } else {
                Renderer.DrawLine({Plot.X, At}, {Plot.X + Plot.W, At}, Look.ColorGridLine, Look.GridLineThicknessLogical);
            }
        }
        if (Labelled) {
            const std::string Label = FormatTick(Tick);
            const float       Width = TextWidth(Label);
            const Point       Where = ValueAxisHorizontal
                                          ? Point{std::max(LastContentRect.X, At - Width * 0.5f), Plot.Y + Plot.H + Gap}
                                          : Point{Plot.X - Gap - Width, At - LineH * 0.5f};
            Renderer.DrawText(Font, Label, Where, Look.ColorLabel);
        }
    });
}

void AxisChartBase::DrawBaseline(Render::Renderer& Renderer) const {
    const ChartStyle& Look = ChartVisualStyle;
    if (!ShowAxes || Look.AxisThicknessLogical <= 0.0f || Look.ColorAxis.A == 0) {
        return;
    }
    const float At   = ValuePosition(BaselineValue());
    const Rect& Plot = PlotRect;
    if (ValueAxisHorizontal) {
        Renderer.DrawLine({At, Plot.Y}, {At, Plot.Y + Plot.H}, Look.ColorAxis, Look.AxisThicknessLogical);
    } else {
        Renderer.DrawLine({Plot.X, At}, {Plot.X + Plot.W, At}, Look.ColorAxis, Look.AxisThicknessLogical);
    }
}

void AxisChartBase::DrawCategoryLabels(Render::Renderer& Renderer, const std::function<float(std::size_t)>& CentreAt,
                                       float Spacing) const {
    const ChartStyle& Look  = ChartVisualStyle;
    const float       LineH = ShowAxes ? LineHeight() : 0.0f;
    if (LineH <= 0.0f || Look.ColorLabel.A == 0 || Labels.empty()) {
        return;
    }
    const float Gap  = Look.LabelGapLogical;
    const Rect& Plot = PlotRect;

    // Label every Nth category, N just big enough that neighbours don't touch.
    float Footprint = LineH;
    if (!ValueAxisHorizontal) {
        Footprint = 0.0f;
        for (const std::string& Label : Labels) {
            Footprint = std::max(Footprint, TextWidth(Label));
        }
        Footprint += Gap;
    }
    const std::size_t Every =
        Spacing > 0.0f ? static_cast<std::size_t>(std::max(1.0f, std::ceil(Footprint / Spacing - TickTolerance))) : 1;

    for (std::size_t Index = 0; Index < Labels.size(); Index += Every) {
        const std::string& Label = Labels[Index];
        const float        Width = TextWidth(Label);
        Point              Where{};
        if (ValueAxisHorizontal) {
            Where = {Plot.X - Gap - Width, CentreAt(Index) - LineH * 0.5f};
        } else {
            // Kept within the plot's width, so the end labels don't run into the value
            // labels or off the chart.
            const float Left = std::min(CentreAt(Index) - Width * 0.5f, Plot.X + Plot.W - Width);
            Where            = {std::max(Plot.X, Left), Plot.Y + Plot.H + Gap};
        }
        Renderer.DrawText(Font, Label, Where, Look.ColorLabel);
    }
}

} // namespace Penumbra::Widgets
