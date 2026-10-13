#pragma once

#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Widgets/Chart.h"

namespace Penumbra::Widgets {

// A line per series through its values, the first category at the plot's left edge
// and the last at its right. ChartStyle adds an area fill down to the baseline
// (AreaOpacity), a dot on each value (PointRadiusLogical) and bigger dots on the
// hovered category (HoverPointRadiusLogical). Lines grow up from the baseline. With
// ShowAxes false it's a sparkline: no axes, and the values span the whole height.
class LineChart : public AxisChartBase {
public:
    Render::LineJoin Join{Render::LineJoin::Round};
    Render::LineCap  Cap{Render::LineCap::Round};

protected:
    void DrawChart(Render::Renderer&, Rect ContentRect, bool Snap, float DeltaSeconds,
                   float AppearProgress) override;
    int  HitTest(Point MouseLogical) const override;
    bool AxisIncludesZero() const override { return ShowAxes; }

private:
    float PointX(std::size_t Category) const;
    float PointSpacing() const;
};

} // namespace Penumbra::Widgets
