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

    // Interaction-state background overrides -- universal (not Button-only) so
    // Lustre's :hover/:active/:disabled selectors have somewhere to land on any
    // classed element, matching how OnPressed/OnHovered/etc. on WidgetBase already
    // aren't Button-exclusive (docs/lustre_style_gaps_requirements.md #1). Zero
    // alpha (the default) means "no override for this state, keep ColorBackground"
    // -- the same presence-flag convention GradientTop/ColorBackground use above.
    Render::Color ColorBackgroundHovered{0, 0, 0, 0};
    Render::Color ColorBackgroundPressed{0, 0, 0, 0};
    Render::Color ColorBackgroundDisabled{0, 0, 0, 0};

    // A whole-subtree paint/hit-test transform (docs/lustre_style_gaps_requirements.md
    // #2) -- Box::Draw composites this Box and every descendant through it as one
    // scaled/rotated/translated blit, and Box::UpdateInteractionState inverse-transforms
    // the mouse point so clicking/hovering tracks the visual position, not the
    // untransformed layout rect. Layout itself (Measure/Arrange, siblings' positions)
    // is unaffected, matching CSS: transform never reflows. One flat value, not a
    // per-state Hovered/Pressed/Disabled trio like the colours above -- resolving e.g.
    // Lustre's `:active { transform: scale(0.97) }` into this field per frame is a
    // resolver-side concern once the primitive exists, not something Penumbra itself
    // needs to track multiple copies of.
    Penumbra::Transform Transform{};
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
