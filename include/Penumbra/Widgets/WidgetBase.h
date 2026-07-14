#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Platform/InputState.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Widgets/Styles.h"

#include <cstddef>

namespace Penumbra::Widgets {

// Four states only — this is complete. If you think you need a fifth, you need a
// different widget.
enum class InteractionState { Default, Hovered, Pressed, Disabled };

class WidgetBase {
public:
    virtual ~WidgetBase() = default;

    // Two-phase layout: Measure reports desired size bottom-up; Arrange commits the
    // final rect top-down.
    virtual Point Measure(Point AvailableSizeLogical) = 0;
    virtual void  Arrange(Rect FinalRectLogical) = 0;

    // Returns true if this widget consumed the input (stops propagation).
    virtual bool UpdateInteractionState(const Platform::InputState&) = 0;

    virtual void Draw(Render::Renderer&) = 0;

    // A child's margin is consumed by its parent during Arrange. Margin lives on
    // BoxStyle, but the parent reaches it polymorphically because every widget is a
    // Box; non-Box widgets (there are none in this PoC) report zero.
    virtual EdgeInsets GetMarginLogical() const { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    // A read-only, uniform way to walk a widget's children regardless of how it
    // stores them internally (a generic vector, named slots, ...). Widgets with no
    // children report zero — the default — rather than every leaf overriding it.
    virtual std::size_t GetChildCount() const { return 0; }
    virtual WidgetBase* GetChildAt(std::size_t Index) const { return nullptr; }

    void SetIsEnabled(bool Enabled) { IsEnabled = Enabled; }
    bool GetIsEnabled() const { return IsEnabled; }
    Rect GetArrangedRect() const { return ArrangedRect; }

    // A widget that is not visible contributes zero size, is not arranged, does not
    // participate in hit-testing, and is not drawn — the same way display:none does.
    void SetIsVisible(bool Visible) { IsVisible = Visible; }
    bool GetIsVisible() const { return IsVisible; }

protected:
    bool IsEnabled{true};
    bool IsVisible{true};
    Rect ArrangedRect{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Penumbra::Widgets
