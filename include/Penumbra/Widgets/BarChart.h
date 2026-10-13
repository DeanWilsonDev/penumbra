#pragma once

#include "Penumbra/Widgets/Chart.h"

namespace Penumbra::Widgets {

// Vertical bars (columns) rise from the baseline, with the categories along the
// bottom; Horizontal bars run right from it, with the categories down the left.
enum class BarOrientation { Vertical, Horizontal };

// One bar per series in each category's band: side by side, or end to end when
// Stacked (positive values stack up from 0, negative ones down). Bars grow in from
// the baseline. Hovering anywhere in a category's band hovers that category.
class BarChart : public AxisChartBase {
public:
    BarOrientation Orientation{BarOrientation::Vertical};
    bool           Stacked{false};

protected:
    void DrawChart(Render::Renderer&, Rect ContentRect, bool Snap, float DeltaSeconds,
                   float AppearProgress) override;
    int  HitTest(Point MouseLogical) const override;
    void DataExtent(float& Low, float& High) const override;

private:
    float BandLength() const; // each category's share of the category axis
    float BandStart(std::size_t Category) const;
};

} // namespace Penumbra::Widgets
