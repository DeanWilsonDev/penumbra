#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Render/Color.h"

namespace Penumbra::Widgets {

struct EdgeInsets {
    float Left;
    float Top;
    float Right;
    float Bottom;
};

enum class LayoutMode { None, VerticalStack, HorizontalStack };
enum class CrossAlign { Start, Center, End, Stretch };

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
    // instead of the flat ColorBackground fill above when GradientTop.A != 0 (docs/
    // penumbra_iris_lustre_componentization_gaps_requirements.md §2 -- Lustre's
    // `background-gradient-start`/`-end`). Zero-alpha (the default) means "no
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

    // Interaction-state background overrides -- universal (not Button-only) so
    // Lustre's :hover/:active/:disabled selectors have somewhere to land on any
    // classed element, matching how OnPressed/OnHovered/etc. on WidgetBase already
    // aren't Button-exclusive. Zero alpha (the default) means "no override for this
    // state, keep ColorBackground"
    // -- the same presence-flag convention GradientTop/ColorBackground use above.
    Render::Color ColorBackgroundHovered{0, 0, 0, 0};
    Render::Color ColorBackgroundPressed{0, 0, 0, 0};
    Render::Color ColorBackgroundDisabled{0, 0, 0, 0};

    // A whole-subtree paint/hit-test transform -- Box::Draw composites this Box
    // and every descendant through it as one
    // scaled/rotated/translated blit, and Box::UpdateInteractionState inverse-transforms
    // the mouse point so clicking/hovering tracks the visual position, not the
    // untransformed layout rect. Layout itself (Measure/Arrange, siblings' positions)
    // is unaffected, matching CSS: transform never reflows. One flat value, not a
    // per-state Hovered/Pressed/Disabled trio like the colours above -- resolving e.g.
    // Lustre's `:active { transform: scale(0.97) }` into this field per frame is a
    // resolver-side concern once the primitive exists, not something Penumbra itself
    // needs to track multiple copies of.
    Penumbra::Transform Transform{};

    // An explicit border-box size override, distinct from Measure's usual
    // content-driven sizing (Lustre's width/height properties). -1 (the default,
    // "auto") means Measure falls through
    // to its normal intrinsic calculation; a value >= 0 is "be exactly this many
    // logical pixels", Padding/BorderWidth included -- the same total Arrange would
    // otherwise have derived from content. Negative rather than a zero/sentinel-alpha
    // convention because 0 is itself a legitimate explicit size (a collapsed spacer).
    // Only the MAIN axis of a Layout::VerticalStack/HorizontalStack parent is
    // guaranteed to honour this on a child -- CrossAlign::Stretch still overrides the
    // CROSS axis unconditionally, the same way it already overrides intrinsic cross
    // sizing today; reconciling explicit-size-wins-over-Stretch is a follow-up, not
    // part of this primitive.
    float WidthLogical{-1.0f};
    float HeightLogical{-1.0f};
};

// Per-widget styles extend BoxStyle so the box-model slots stay universal and free.
struct ButtonStyle : BoxStyle {
    Render::Color ColorLabel{0, 0, 0, 0}; // applied to a Label child by the resolver, not by Button
};

struct CheckboxStyle : BoxStyle {
    Render::Color ColorCheckMark{0, 0, 0, 0};
    Render::Color ColorBoxChecked{0, 0, 0, 0};
};

} // namespace Penumbra::Widgets
