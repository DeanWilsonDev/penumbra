#include "Penumbra/Widgets/DonutChart.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Penumbra::Widgets {

namespace {

// DrawArc measures from 3 o'clock; a donut starts at 12.
constexpr float TwelveOClockDegrees = -90.0f;

} // namespace

void DonutChart::DrawChart(Render::Renderer& Renderer, Rect ContentRect, bool Snap, float DeltaSeconds,
                           float AppearProgress) {
    const ChartStyle& Look = ChartVisualStyle;

    std::vector<float> Target;
    Target.reserve(Slices.size());
    for (const ChartSlice& Slice : Slices) {
        Target.push_back(std::max(0.0f, Slice.Value));
    }
    SyncValues(Values, Target, Snap, DeltaSeconds);

    Centre      = {ContentRect.X + ContentRect.W * 0.5f, ContentRect.Y + ContentRect.H * 0.5f};
    OuterRadius = std::min(ContentRect.W, ContentRect.H) * 0.5f;
    InnerRadius = Look.DonutThicknessLogical > 0.0f ? std::max(0.0f, OuterRadius - Look.DonutThicknessLogical) : 0.0f;

    float       Total   = 0.0f;
    std::size_t Visible = 0;
    for (std::size_t Index = 0; Index < Values.Size(); ++Index) {
        Total += std::max(0.0f, Values.ValueAt(Index));
        Visible += Values.ValueAt(Index) > 0.0f ? 1 : 0;
    }

    SliceStarts.assign(Values.Size(), 0.0f);
    SliceSweeps.assign(Values.Size(), 0.0f);
    if (Total > 0.0f) {
        // One slice alone is a whole ring, with no gap to cut into it.
        const float Gap   = Visible > 1 ? std::max(0.0f, Look.SegmentGapDegrees) : 0.0f;
        float       Angle = 0.0f;
        for (std::size_t Index = 0; Index < Values.Size() && Index < Slices.size(); ++Index) {
            const float Sweep  = std::max(0.0f, Values.ValueAt(Index)) / Total * 360.0f * AppearProgress;
            SliceStarts[Index] = Angle;
            SliceSweeps[Index] = Sweep;
            if (Sweep > Gap) {
                const float Start = TwelveOClockDegrees + Angle + Gap * 0.5f;
                Renderer.DrawArc(Centre, InnerRadius, OuterRadius, Start, Sweep - Gap, Slices[Index].SliceColor);
                if (static_cast<int>(Index) == GetHoveredIndex() && Look.ColorHover.A != 0) {
                    Renderer.DrawArc(Centre, InnerRadius, OuterRadius, Start, Sweep - Gap, Look.ColorHover);
                }
            }
            Angle += Sweep;
        }
    }

    // The centre text, as one block of up to two lines, centred.
    if (!FontBackend || Look.ColorLabel.A == 0 || (CentreText.empty() && CentreCaption.empty())) {
        return;
    }
    const Render::FontHandle MainFont = CentreFont.value_or(Font);
    const float MainHeight = CentreText.empty() ? 0.0f : FontBackend->MeasureText(MainFont, "Ag").HeightLogical;
    const float CaptionHeight = CentreCaption.empty() ? 0.0f : LineHeight();
    float       Top           = Centre.Y - (MainHeight + CaptionHeight) * 0.5f;
    if (!CentreText.empty()) {
        const float Width = FontBackend->MeasureTextWidth(MainFont, CentreText);
        Renderer.DrawText(MainFont, CentreText, {Centre.X - Width * 0.5f, Top}, Look.ColorLabel);
        Top += MainHeight;
    }
    if (!CentreCaption.empty()) {
        Renderer.DrawText(Font, CentreCaption, {Centre.X - TextWidth(CentreCaption) * 0.5f, Top}, Look.ColorLabel);
    }
}

int DonutChart::HitTest(Point MouseLogical) const {
    const float OffsetX  = MouseLogical.X - Centre.X;
    const float OffsetY  = MouseLogical.Y - Centre.Y;
    const float Distance = std::sqrt(OffsetX * OffsetX + OffsetY * OffsetY);
    if (Distance > OuterRadius || Distance < InnerRadius) {
        return -1;
    }

    // Degrees clockwise from 12 o'clock (y runs down, so atan2 already turns clockwise).
    float Angle = std::atan2(OffsetY, OffsetX) * 180.0f / std::numbers::pi_v<float> - TwelveOClockDegrees;
    if (Angle < 0.0f) {
        Angle += 360.0f;
    }
    for (std::size_t Index = 0; Index < SliceStarts.size(); ++Index) {
        if (SliceSweeps[Index] > 0.0f && Angle >= SliceStarts[Index] && Angle < SliceStarts[Index] + SliceSweeps[Index]) {
            return static_cast<int>(Index);
        }
    }
    return -1;
}

} // namespace Penumbra::Widgets
