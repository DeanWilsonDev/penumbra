#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Render/Color.h"
#include "Penumbra/Widgets/Length.h"

namespace Penumbra::Widgets {

struct EdgeInsets {
    float Left;
    float Top;
    float Right;
    float Bottom;
};

// FixedLeadingStack: children[0] ("Leading") gets exactly Box::LeadingExtentLogical
// along the main axis (Y), regardless of its own measured size; every subsequent
// child ("Fill") shares whatever's left after Leading + ChildGap. Unlike
// VerticalStack/HorizontalStack, this mode's own Box::Measure() reports back the
// full available size (greedy) rather than the sum of its children's sizes --
// allowing a fixed-height leading child (a
// toolbar, a header) above a greedy fill child (a SplitPanel/ViewportWidget,
// which itself reports "whatever I'm given" as its own desired size) can't be
// expressed by VerticalStack, which hands every child the same undiminished
// ContentAvailable regardless of a preceding sibling (see docs/next_steps.md).
enum class LayoutMode { None, VerticalStack, HorizontalStack, FixedLeadingStack };
enum class CrossAlign { Start, Center, End, Stretch };

// Main-axis distribution for VerticalStack/HorizontalStack, parallel to CrossAlign's
// cross-axis alignment above. Start is the default and reproduces sequential packing
// (sequential packing from Content's main-axis start, ChildGap between siblings, no other
// change) -- Center/End/SpaceBetween are opt-in via Box::JustifyContentMode. Not
// meaningful for FixedLeadingStack (that layout's two slots are sized directly by
// LeadingExtentLogical/the remainder, not distributed) or LayoutMode::None (no children
// laid out at all). No SpaceAround/SpaceEvenly -- not requested by any real consumer;
// This is intentionally not a full flexbox-equivalent distribution model.
enum class Justify { Start, Center, End, SpaceBetween };

// The universal style slots — the "tokens" every widget honours. Penumbra defines
// the SHAPE; it supplies NO values and NO semantic names. Default-constructed it is
// all-zero (transparent, no border, no spacing): the absence of styling, not an
// opinion. The demo fills in every value it cares about.
struct BoxStyle {
    Render::Color ColorBackground{0, 0, 0, 0};
    Render::Color ColorBorder{0, 0, 0, 0};
    float         BorderWidth{0.0f};
    float         BorderRadius{0.0f}; // honoured by API; rendered square in first cut
    EdgeInsets    Padding{0.0f, 0.0f, 0.0f, 0.0f}; // inside the border  — the box's own job
    EdgeInsets    Margin{0.0f, 0.0f, 0.0f, 0.0f};  // outside the border — the PARENT's job

    // A top-to-bottom two-stop gradient fill, drawn via Renderer::DrawGradientRect
    // instead of the flat ColorBackground fill above when GradientTop.A != 0.
    // Zero-alpha (the default) means "no
    // gradient, use ColorBackground" -- the same "alpha is the presence flag"
    // convention ColorBackground/ColorBorder above already use, so a Box with
    // neither set still draws nothing extra.
    Render::Color GradientTop{0, 0, 0, 0};
    Render::Color GradientBottom{0, 0, 0, 0};

    // Per-state gradient overrides, same presence-flag convention as GradientTop/
    // ColorBackgroundHovered above (alpha 0 = "no override, use GradientTop/
    // GradientBottom"). No Disabled variant -- every known consumer falls back to
    // a flat ColorBackgroundDisabled fill when disabled, not a disabled-state
    // gradient.
    Render::Color GradientTopHovered{0, 0, 0, 0};
    Render::Color GradientBottomHovered{0, 0, 0, 0};
    Render::Color GradientTopPressed{0, 0, 0, 0};
    Render::Color GradientBottomPressed{0, 0, 0, 0};

    // A soft rectangular shadow drawn just before this Box's own background/
    // gradient fill, mirroring Renderer::DrawDropShadow's two-argument shape
    // (Render/Renderer.h:80-81). 0 blur radius (the default) draws no shadow --
    // same presence-flag convention as everything else in this struct.
    Render::Color ShadowColor{0, 0, 0, 0};
    float         ShadowBlurRadiusLogical{0.0f};

    // Interaction-state background overrides are universal (not Button-only),
    // matching how OnPressed/OnHovered/etc. on WidgetBase already
    // aren't Button-exclusive. Zero alpha (the default) means "no override for this
    // state, keep ColorBackground"
    // -- the same presence-flag convention GradientTop/ColorBackground use above.
    Render::Color ColorBackgroundHovered{0, 0, 0, 0};
    Render::Color ColorBackgroundPressed{0, 0, 0, 0};
    Render::Color ColorBackgroundDisabled{0, 0, 0, 0};

    // Interaction-state border-color overrides, same presence-flag convention and
    // rationale as ColorBackgroundHovered/Pressed/Disabled above.
    Render::Color ColorBorderHovered{0, 0, 0, 0};
    Render::Color ColorBorderPressed{0, 0, 0, 0};
    Render::Color ColorBorderDisabled{0, 0, 0, 0};

    // A whole-subtree paint/hit-test transform -- Box::Draw composites this Box
    // and every descendant through it as one
    // scaled/rotated/translated blit, and Box::UpdateInteractionState inverse-transforms
    // the mouse point so clicking/hovering tracks the visual position, not the
    // untransformed layout rect. Layout itself (Measure/Arrange, siblings' positions)
    // is unaffected, matching CSS: transform never reflows. One flat value, not a
    // per-state Hovered/Pressed/Disabled trio like the colours above. Resolving
    // state-specific transforms is a consumer concern.
    Penumbra::Transform Transform{};

    Length Width;
    Length Height;
    Length MinWidth;
    Length MaxWidth;
    float  FlexGrow{0.0f};
};

// Per-widget styles extend BoxStyle so the box-model slots stay universal and free.
struct ButtonStyle : BoxStyle {
    Render::Color ColorLabel{0, 0, 0, 0}; // applied to a Label child by the consumer, not by Button
};

struct CheckboxStyle : BoxStyle {
    Render::Color ColorCheckMark{0, 0, 0, 0};
    Render::Color ColorBoxChecked{0, 0, 0, 0};
};

// All values are consumer supplied. DropdownStyle separates the trigger's ordinary
// BoxStyle slice from the independently-painted popover and row surfaces.
struct DropdownStyle : BoxStyle {
    BoxStyle      Menu{};
    BoxStyle      Row{};
    BoxStyle      SelectedRow{};
    Render::Color ColorText{0, 0, 0, 0};
    Render::Color ColorTextHighlighted{0, 0, 0, 0};
    float         TriggerContentSizeLogical{0.0f};
    float         RowHeightLogical{0.0f};
    float         LeadingVisualSizeLogical{0.0f};
    float         LeadingVisualGapLogical{0.0f};
    float         MinimumOpenWidthLogical{0.0f};
    float         OpenTransitionSeconds{0.0f};
};

// The slots DonutChart, BarChart and LineChart share. Every value is the consumer's;
// a zero size or a zero-alpha colour draws nothing, as everywhere else here.
struct ChartStyle : BoxStyle {
    Render::Color ColorAxis{0, 0, 0, 0};     // the baseline, at value 0
    Render::Color ColorGridLine{0, 0, 0, 0}; // one line per value tick
    Render::Color ColorLabel{0, 0, 0, 0};    // value, category and donut-centre text
    // Behind the hovered category in a BarChart or LineChart (a band, or a guide line
    // GridLineThicknessLogical wide), over the hovered slice in a DonutChart.
    Render::Color ColorHover{0, 0, 0, 0};

    float AxisThicknessLogical{0.0f};
    float GridLineThicknessLogical{0.0f};
    // The least distance between value ticks. The chart steps its value axis in
    // 1, 2 or 5 x 10^n, the smallest such step that keeps ticks this far apart (and
    // horizontal value labels clear of each other). 0 draws no ticks, grid lines or
    // value labels.
    float TickSpacingLogical{0.0f};
    float LabelGapLogical{0.0f}; // between the plot and its labels, and between labels

    float LineThicknessLogical{0.0f};    // LineChart
    float PointRadiusLogical{0.0f};      // LineChart: a dot on every value
    float HoverPointRadiusLogical{0.0f}; // LineChart: the dots of the hovered category
    float AreaOpacity{0.0f};             // LineChart: the fill under each line, as a fraction of its colour's alpha

    float BarGapFraction{0.0f}; // BarChart: the share of each category's band left empty
    float BarCornerRadiusLogical{0.0f};

    float DonutThicknessLogical{0.0f}; // DonutChart: the ring's width; 0 fills to the centre, a pie
    float SegmentGapDegrees{0.0f};     // DonutChart: the gap between neighbouring slices

    float AppearSeconds{0.0f};     // growing in each time the chart is shown
    float TransitionSeconds{0.0f}; // easing from one data set to the next
};

} // namespace Penumbra::Widgets
