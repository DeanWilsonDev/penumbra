#pragma once

#include "Penumbra/Platform/InputState.h"
#include "Penumbra/Render/Renderer.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Penumbra::Widgets {

// CSS's stacking contexts, for BoxStyle's PositionMode and ZIndex.
//
// A stacking context forms at the first Box drawn, or handed input, while none is
// active (the root of whatever is being drawn), and at every widget with a z-index, a
// Fixed or Sticky position, or an opacity below 1 or a transform (PaintsAsGroup). Each
// pass it collects its layered descendants: those that are positioned, have a z-index,
// or paint as a group. It looks inside positioned widgets without a z-index, whose
// layered descendants are its own too, as in CSS, but not inside nested contexts.
//
// It then paints in CSS's order: the root's own background and content, negative
// z-index ascending, ordinary content in tree order, z-index 0 and auto in tree order,
// then positive z-index ascending. Each layered widget is clipped by its containing
// block's ancestors, not by every ancestor: by its parent's clip if it's in flow, its
// containing block's if it's Absolute, and nothing if it's Fixed. Input runs in the
// reverse of that order, and a layered widget takes the pointer over its whole rect, so
// nothing painted under it is hovered or pressed through it.
//
// A container skips a child that IsLayeredOut, both when drawing and when handing out
// input: its stacking context does that child itself, later. Box and ScrollablePanel
// set one up in Draw and UpdateInteractionState; a custom container that draws its own
// children needs only the IsLayeredOut check.
class StackingContext {
public:
    enum class Pass { Paint, Input };

    // Starts a context at Root if Root forms one, or if no context is active. Otherwise
    // it's inactive and every call below does nothing.
    StackingContext(WidgetBase& Root, Pass Kind);
    ~StackingContext();

    StackingContext(const StackingContext&)            = delete;
    StackingContext& operator=(const StackingContext&) = delete;

    bool IsActive() const { return Active; }

    // Paint: the layered widgets under the root's ordinary content (negative z-index),
    // and those over it.
    void PaintBelow(Render::Renderer&);
    void PaintAbove(Render::Renderer&);

    // Input, in reverse paint order: DispatchAbove before the root's ordinary children,
    // returning whether a layered widget took the pointer; DispatchBelow after them,
    // with whether anything has taken it so far, returning whether one of those did.
    bool DispatchAbove(const Platform::InputState& Input);
    bool DispatchBelow(const Platform::InputState& Input, bool PointerTaken);

    // Whether Widget's parent should skip it in tree order this pass, because the active
    // context paints it, or hands it input, itself.
    static bool IsLayeredOut(const WidgetBase& Widget);

    // Whether Widget lies between the active context's root and the layered widget that
    // took the pointer this pass. Such a widget still sees the pointer over itself, as
    // an ancestor of a hovered element does in CSS, so it keeps its own hover and its
    // wheel scrolling; the press belongs to the descendant.
    static bool IsOnPointerPath(const WidgetBase& Widget);

    // Whether the layered widget that took the pointer this pass used the wheel itself.
    static bool PointerTakerUsedWheel();

    // Whether Widget forms a stacking context of its own (a z-index, Fixed, Sticky, or
    // PaintsAsGroup).
    static bool FormsContext(const WidgetBase& Widget);

private:
    // How a layered widget is clipped when it's painted: within the root's clip (and
    // Area, if any), or, when Replace, by Area alone, or nothing.
    struct Clip {
        bool                Replace{false};
        std::optional<Rect> Area;
    };

    struct Entry {
        WidgetBase*                    Widget{nullptr};
        int                            Z{0};
        std::size_t                    Order{0};
        Clip                           Clipping;
        std::optional<bool>            PassThroughAbove;        // the pointer-events its parent hands down; nullopt: the root's

        std::vector<const WidgetBase*> Ancestors;               // from below the root down to its parent
    };

    struct Walk;

    void Collect(const WidgetBase& Parent, Walk& State);
    void Paint(Render::Renderer&, const Entry&);
    bool Dispatch(const Entry&, const Platform::InputState& Input, bool PointerTaken);

    bool                           Active{false};
    Pass                           Kind{Pass::Paint};
    std::uint64_t                  Serial{0};
    WidgetBase*                    Root{nullptr};
    const WidgetBase*              Current{nullptr}; // the layered widget being painted or dispatched
    std::vector<Entry>             Entries;          // by z-index, then tree order
    std::vector<const WidgetBase*> PointerPath;
    bool                           TakerUsedWheel{false};
};

} // namespace Penumbra::Widgets
