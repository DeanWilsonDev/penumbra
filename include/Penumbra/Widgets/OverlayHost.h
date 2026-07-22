#pragma once

#include "Penumbra/Widgets/WidgetBase.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Penumbra::Widgets {

// Opaque handle to a shown overlay, returned by ShowOverlay so the caller can
// dismiss that specific overlay later (e.g. a dropdown closing itself once the
// user picks an item). Never reused within one OverlayHost's lifetime.
using OverlayId = std::size_t;

// The seam that lets a widget buried anywhere in the tree (a dropdown Button, a
// hover tooltip) paint above every sibling and ancestor and get first refusal on
// input, without the framework threading a z-index through every Measure/Arrange
// call. An OverlayHost wraps one "Root" subtree (the app's ordinary widget tree)
// plus a stack of overlays placed at explicit rects; it is itself a WidgetBase, so
// a consumer drops it in wherever it previously drove Root directly (see
// demo/main.cpp) and gets the same Measure/Arrange/UpdateInteractionState/Draw
// call shape.
//
// An overlay is not part of Root's layout: ShowOverlay takes an explicit
// PlacementRectLogical (the caller already knows this — e.g. a dropdown anchors
// below the button that opened it, via that button's GetArrangedRect()) rather
// than being measured/arranged by a parent Box. Because Root->Draw() and every
// clip/transform push it does are fully popped before OverlayHost draws the next
// overlay, overlay content is never inside Root's clip rect — the "escape the
// parent's clip" that a popup needs falls out of draw order, not a new Renderer
// primitive.
//
// A popup/menu-overlay layer was originally deferred as not requested (Pharos
// didn't need one); this is that layer, now that a real consumer
// (`ColorFilterDropdown`) does.
class OverlayHost : public WidgetBase {
public:
    // Replaces the wrapped subtree. Not meant to be called after the first
    // Arrange (same one-time-setup expectation as SplitPanel::SetFirst/SetSecond).
    WidgetBase* SetRoot(std::unique_ptr<WidgetBase> Content);

    // Stacks Content above Root and every previously-shown overlay, both for draw
    // order (later = on top) and input priority (later = first refusal). Content
    // is measured and arranged at PlacementRectLogical directly — it does not go
    // through Root's layout. DismissOnOutsideClick: a press outside
    // PlacementRectLogical closes this overlay and consumes the event instead of
    // falling through to whatever is beneath it — the standard dropdown/menu
    // convention (set false for e.g. a non-modal tooltip that shouldn't eat
    // clicks meant for the page).
    OverlayId ShowOverlay(std::unique_ptr<WidgetBase> Content, Rect PlacementRectLogical,
                         bool DismissOnOutsideClick = true);

    // Updates a shown overlay's placement rect in place (e.g. a width-open tween
    // recomputing the panel rect every frame) without disturbing its Content or
    // its position in the stacking order. Takes effect on the next Arrange — the
    // new rect isn't measured/arranged immediately, matching every other Arrange-
    // driven rect in the tree. No-op if Id doesn't name a currently-shown overlay.
    void SetOverlayPlacement(OverlayId Id, Rect PlacementRectLogical);

    // No-op if Id doesn't name a currently-shown overlay (already dismissed, or
    // never existed) — callers don't need to track whether they already closed it.
    void DismissOverlay(OverlayId Id);
    void DismissAll();
    bool HasOverlays() const { return !Overlays.empty(); }

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    // Root (if set) at index 0, then each open overlay in stacking order — so
    // generic tree-walking tools (a future debug inspector) see the whole picture
    // rather than just Root.
    std::size_t GetChildCount() const override;
    WidgetBase* GetChildAt(std::size_t Index) const override;

private:
    struct Overlay {
        OverlayId                   Id;
        std::unique_ptr<WidgetBase> Content;
        Rect                        PlacementRectLogical;
        bool                        DismissOnOutsideClick;
    };

    std::unique_ptr<WidgetBase> Root;
    std::vector<Overlay>        Overlays;
    OverlayId                   NextId{1};
};

} // namespace Penumbra::Widgets
