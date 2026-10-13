#pragma once

#include "Penumbra/Widgets/Chart.h"

#include <optional>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

// A ring (or, with ChartStyle::DonutThicknessLogical 0, a pie) cut into one slice
// per entry, each sized by its share of the total and starting at 12 o'clock,
// clockwise. It fills the largest circle that fits the content rect. A negative value
// counts as 0. It grows in by sweeping round from 12 o'clock.
class DonutChart : public ChartBase {
public:
    std::vector<ChartSlice> Slices;

    // Up to two lines centred in the hole: CentreText in CentreFont (Font when
    // unset), say a total or a percentage, and CentreCaption in Font beneath it.
    std::string                       CentreText;
    std::string                       CentreCaption;
    std::optional<Render::FontHandle> CentreFont;

protected:
    void DrawChart(Render::Renderer&, Rect ContentRect, bool Snap, float DeltaSeconds,
                   float AppearProgress) override;
    int  HitTest(Point MouseLogical) const override;

private:
    Anim::TweenedValues Values;

    // From the last draw, for HitTest. Angles are degrees clockwise from 12 o'clock.
    Point              Centre{};
    float              InnerRadius{0.0f};
    float              OuterRadius{0.0f};
    std::vector<float> SliceStarts;
    std::vector<float> SliceSweeps;
};

} // namespace Penumbra::Widgets
