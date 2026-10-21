#pragma once

#include "Penumbra/Geometry.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <cstddef>

namespace Penumbra::Widgets {

// Layout for BoxStyle's PositionMode, as CSS lays out position. Box and ScrollablePanel
// use these; another container that lays out its own children can too.

// Whether Child leaves its parent's layout: visible, and Absolute or Fixed. Its parent
// measures, sizes and stacks its other children as if it weren't there, then places it
// with ArrangeOutOfFlow.
bool IsOutOfFlow(const WidgetBase& Child);

// InFlowRect moved by a Relative child's offsets, as CSS moves it after layout, with
// percentages taken of ParentContent. Any other child's rect comes back unchanged.
Rect OffsetInFlow(const WidgetBase& Child, Rect InFlowRect, Rect ParentContent);

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

} // namespace Penumbra::Widgets
