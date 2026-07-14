#pragma once

#include "Penumbra/Widgets/Box.h"

#include <memory>

namespace Penumbra::Widgets {

enum class SplitAxis { Horizontal, Vertical }; // Horizontal = divider runs vertically, splits left/right

struct SplitPanelStyle : BoxStyle {
    Render::Color ColorHandle{0, 0, 0, 0};
    Render::Color ColorHandleHovered{0, 0, 0, 0};
    Render::Color ColorHandleDragged{0, 0, 0, 0};
};

// Exactly two children, sized by a draggable ratio rather than their own
// Measure(). Mirrors NumericDrag's click-and-drag idiom (internal Dragging
// bool + last-pointer-position, consuming input regardless of hover once a
// drag has started) but drives a split ratio instead of a scalar value.
class SplitPanel : public Box {
public:
    SplitAxis Axis{SplitAxis::Horizontal};
    float     SplitRatio{0.5f};           // 0..1, fraction given to the first child
    float     HandleThicknessLogical{0.0f};
    float     MinPaneSizeLogical{0.0f};   // clamps SplitRatio so neither pane collapses

    // First/Second replace Box::Children for this widget's two slots; Box's
    // generic Children vector is unused here (AddChild is not meaningful).
    WidgetBase* SetFirst(std::unique_ptr<WidgetBase> Child);
    WidgetBase* SetSecond(std::unique_ptr<WidgetBase> Child);

    void ApplyStyle(const SplitPanelStyle& Style);

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    // Reports First/Second through the generic child-enumeration contract instead
    // of Box's Children vector, which this widget doesn't use.
    std::size_t GetChildCount() const override;
    WidgetBase* GetChildAt(std::size_t Index) const override;

private:
    // Handle state colours, poured in by ApplyStyle — same pattern as Button's own
    // state-colour fields fed from ButtonStyle.
    Render::Color ColorHandle{0, 0, 0, 0};
    Render::Color ColorHandleHovered{0, 0, 0, 0};
    Render::Color ColorHandleDragged{0, 0, 0, 0};

    std::unique_ptr<WidgetBase> First;
    std::unique_ptr<WidgetBase> Second;
    Rect                        HandleRect{};
    bool                        Dragging{false};
    bool                        HandleHovered{false};
    float                       LastPointerMain{0.0f}; // x for Horizontal, y for Vertical
};

} // namespace Penumbra::Widgets
