#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <cstddef>
#include <optional>

namespace Penumbra::Widgets {

// Layout for BoxStyle's PositionMode, as CSS lays out position. Box and ScrollablePanel
// use these; another container that lays out its own children can too.

// Whether Child leaves its parent's layout: visible, and Absolute or Fixed. Its parent
// measures, sizes and stacks its other children as if it weren't there, then places it
// with ArrangeOutOfFlow.
bool IsOutOfFlow(const WidgetBase& Child);

// InFlowRect moved after layout, as CSS moves it, for a Relative or Sticky child; any
// other child's rect comes back unchanged.
//
// Relative moves by its offsets, percentages taken of ParentContent. Sticky stays put
// until the innermost scrollport (see ScrollportScope) has scrolled it past an offset,
// then holds at that distance from the scrollport's edge (percentages of the
// scrollport), but never leaves StickyBlock, ParentContent unless given: a ScrollablePanel
// passes its whole scrolled content, so a child of it sticks for the full scroll range.
Rect OffsetInFlow(const WidgetBase& Child, Rect InFlowRect, Rect ParentContent,
                  std::optional<Rect> StickyBlock = std::nullopt);

// Measures and arranges an Absolute or Fixed Child against its containing block (see
// ContainingBlockScope). StaticPosition is where its border box would have started in
// flow, used on an axis where both offsets are auto, as CSS's static position is.
void ArrangeOutOfFlow(WidgetBase& Child, Point StaticPosition);

// Makes Widget the containing block of the descendants it arranges while this lives, as
// far as it is one: when it's positioned, for Absolute descendants, and when it has a
// transform, for Fixed ones too. The outermost one in a pass also sets up the initial
// containing block for both: the window, from SetLayoutViewportLogical, or the outermost
// widget's own box when the host hasn't set that. Construct it in Arrange before
// arranging children; PaddingBox is Widget's arranged rect inside its border.
class ContainingBlockScope {
public:
    ContainingBlockScope(const WidgetBase& Widget, Rect PaddingBox);
    ~ContainingBlockScope();

    ContainingBlockScope(const ContainingBlockScope&)            = delete;
    ContainingBlockScope& operator=(const ContainingBlockScope&) = delete;

private:
    std::size_t Pushed{0};
};

// Makes Scrollport, a scroll container's visible content box, where Sticky descendants
// arranged while this lives stick to. Construct it in a scroll container's Arrange before
// arranging its children; with none, the window is the scrollport.
class ScrollportScope {
public:
    explicit ScrollportScope(Rect Scrollport);
    ~ScrollportScope();

    ScrollportScope(const ScrollportScope&)            = delete;
    ScrollportScope& operator=(const ScrollportScope&) = delete;
};

} // namespace Penumbra::Widgets
