#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Platform/InputState.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Widgets/Styles.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Penumbra::Widgets {

class StackingContext;

// Four states only — this is complete. If you think you need a fifth, you need a
// different widget.
enum class InteractionState { Default, Hovered, Pressed, Disabled };

class WidgetBase {
public:
    virtual ~WidgetBase() {
        if (OnDestroyed) {
            OnDestroyed();
        }
    }

    // Two-phase layout: Measure reports desired size bottom-up; Arrange commits the
    // final rect top-down.
    virtual Point Measure(Point AvailableSizeLogical) = 0;
    virtual void  Arrange(Rect FinalRectLogical) = 0;

    // Returns true if this widget consumed the input (stops propagation).
    virtual bool UpdateInteractionState(const Platform::InputState&) = 0;

    // Whether this widget itself used a mouse-wheel scroll on ITSELF this frame --
    // distinct from UpdateInteractionState's general "consumed" return, which also
    // covers plain hover/click and would otherwise make an ancestor ScrollablePanel
    // think a nested wheel-scrollable region (another ScrollablePanel, or a TextArea)
    // already claimed the wheel even when that region had no room left to scroll (and
    // so should let the wheel fall through to the ancestor instead of getting stuck).
    // False by default; only a widget with its own WheelStepLogical-style field needs
    // to override it.
    virtual bool ConsumedWheelThisFrame() const { return false; }

    virtual void Draw(Render::Renderer&) = 0;

    virtual EdgeInsets GetMarginLogical() const { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    virtual float      GetFlexGrow() const { return 0.0f; }

    // The box-model and positioning slots, for a widget that has them (every Box), so a
    // container or a stacking context can read a child's position, offsets, z-index and
    // pointer-events. Null, the default, is a static, auto widget.
    virtual const BoxStyle* GetBoxStyle() const { return nullptr; }

    // Whether this widget composites its whole subtree as one group (an opacity below 1
    // or a transform), which makes it a stacking context, as in CSS.
    virtual bool PaintsAsGroup() const { return false; }

    // The rect this widget clips its children to, if it clips them (a ScrollablePanel's
    // viewport). A stacking context clips a positioned descendant it paints later with
    // the clips of the right ancestors (see Stacking.h).
    virtual std::optional<Rect> GetChildClipRect() const { return std::nullopt; }

    // A read-only, uniform way to walk a widget's children regardless of how it
    // stores them internally (a generic vector, named slots, ...). Widgets with no
    // children report zero — the default — rather than every leaf overriding it.
    virtual std::size_t GetChildCount() const { return 0; }
    virtual WidgetBase* GetChildAt(std::size_t Index) const { return nullptr; }

    // Generic pointer/focus/value callbacks any widget can opt into, including a
    // plain Box rather than only dedicated interactive subclasses. All
    // null by default: a widget with none set is exactly as inert as before this
    // existed, and dispatch skips hit-testing entirely rather than paying for it
    // (see Box::UpdateInteractionState).
    std::function<void()> OnPressed  = nullptr;
    std::function<void()> OnReleased = nullptr;
    std::function<void()> OnHovered  = nullptr;
    std::function<void()> OnFocused  = nullptr;
    std::function<void()> OnChanged  = nullptr;

    // Fires from ~WidgetBase() when set -- the generic "this widget is being torn
    // down" hook a plain Box handed out via BuildWidgetTree can opt into, mirroring
    // OnPressed/etc.'s "null by default, opt-in" convention. Lets a caller detect
    // host-driven removal (e.g. OverlayHost erasing this widget from its stack)
    // without needing a hand-rolled subclass whose sole purpose is running cleanup
    // in its own destructor.
    std::function<void()> OnDestroyed = nullptr;

    // Inert class-name storage. Penumbra holds the string and does nothing with
    // it, consistent with "no defaults, no opinions"; interpretation belongs to
    // the consumer.
    std::string ClassName;

    void SetIsEnabled(bool Enabled) { IsEnabled = Enabled; }
    bool GetIsEnabled() const { return IsEnabled; }
    Rect GetArrangedRect() const { return ArrangedRect; }

    // A widget that is not visible contributes zero size, is not arranged, does not
    // participate in hit-testing, and is not drawn — the same way display:none does.
    // Showing it again calls OnShown on it and its visible descendants.
    void SetIsVisible(bool Visible) {
        const bool Shown = Visible && !IsVisible;
        IsVisible        = Visible;
        if (Shown) {
            NotifyShown();
        }
    }
    bool GetIsVisible() const { return IsVisible; }

protected:
    // Called when this widget, or a hidden ancestor, is shown again: the moment CSS
    // applies @starting-style after display: none. A Box replays its enter
    // transitions here.
    virtual void OnShown() {}

    bool IsEnabled{true};
    bool IsVisible{true};
    Rect ArrangedRect{0.0f, 0.0f, 0.0f, 0.0f};

private:
    friend class StackingContext;

    // The stacking context that paints, or dispatches input to, this widget itself this
    // pass, rather than its parent in tree order (see Stacking.h), 0 when none does, and
    // whether that context found it forms one of its own. Its own context forms from
    // this, not from its style when it's painted, so the two agree even when an eased
    // opacity crosses 1 in between.
    std::uint64_t LayeredBy{0};
    bool          LayeredAsContext{false};

    void NotifyShown() {
        OnShown();
        for (std::size_t Index = 0; Index < GetChildCount(); ++Index) {
            WidgetBase* Child = GetChildAt(Index);
            if (Child && Child->IsVisible) {
                Child->NotifyShown();
            }
        }
    }
};

} // namespace Penumbra::Widgets
