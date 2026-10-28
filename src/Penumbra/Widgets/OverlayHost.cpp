#include "Penumbra/Widgets/OverlayHost.h"

#include <algorithm>

namespace Penumbra::Widgets {

namespace {

bool PointInRect(Point P, Rect R) {
    return P.X >= R.X && P.X < R.X + R.W && P.Y >= R.Y && P.Y < R.Y + R.H;
}

} // namespace

WidgetBase* OverlayHost::SetRoot(std::unique_ptr<WidgetBase> Content) {
    Root = std::move(Content);
    return Root.get();
}

OverlayId OverlayHost::ShowOverlay(std::unique_ptr<WidgetBase> Content, std::optional<Rect> PlacementRectLogical,
                                   bool DismissOnOutsideClick) {
    const OverlayId Id = NextId++;
    Overlays.push_back({Id, std::move(Content), PlacementRectLogical, DismissOnOutsideClick});
    return Id;
}

void OverlayHost::SetOverlayPlacement(OverlayId Id, std::optional<Rect> PlacementRectLogical) {
    for (Overlay& Entry : Overlays) {
        if (Entry.Id == Id) {
            Entry.PlacementRectLogical = PlacementRectLogical;
            return;
        }
    }
}

void OverlayHost::DismissOverlay(OverlayId Id) {
    if (DispatchingOverlayInput) {
        PendingDismissals.push_back(Id);
        return;
    }
    Overlays.erase(std::remove_if(Overlays.begin(), Overlays.end(),
                                  [Id](const Overlay& Entry) { return Entry.Id == Id; }),
                  Overlays.end());
}

void OverlayHost::DismissAll() {
    Overlays.clear();
}

Point OverlayHost::Measure(Point AvailableSizeLogical) {
    // Overlays don't participate in this — they're sized/placed explicitly by
    // ShowOverlay's caller, not by whatever offered OverlayHost its available size.
    return Root ? Root->Measure(AvailableSizeLogical) : AvailableSizeLogical;
}

void OverlayHost::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;
    if (Root) {
        Root->Arrange(FinalRectLogical);
    }
    for (Overlay& Entry : Overlays) {
        const Rect Placement = PlacementOf(Entry);
        Entry.Content->Measure({Placement.W, Placement.H});
        Entry.Content->Arrange(Placement);
    }
}

bool OverlayHost::UpdateInteractionState(const Platform::InputState& Input) {
    if (Overlays.empty()) {
        return Root ? Root->UpdateInteractionState(Input) : false;
    }

    // Only the topmost overlay is live — matches ShowOverlay's stacking-order
    // contract and keeps "one click closes one popup" simple, the same
    // convention practically every menu/dropdown implementation uses.
    Overlay& Top = Overlays.back();
    const OverlayId TopId = Top.Id;
    const Rect TopPlacement = PlacementOf(Top);
    const bool TopDismissesOutside = Top.DismissOnOutsideClick;

    const bool EscapePressed = std::ranges::contains(Input.KeysPressedThisFrame, Platform::Key::Escape);
    if (EscapePressed) {
        DismissOverlay(TopId);
    } else {
        DispatchingOverlayInput = true;
        const bool ContentConsumed = Top.Content->UpdateInteractionState(Input);
        DispatchingOverlayInput = false;
        for (const OverlayId Id : PendingDismissals) {
            DismissOverlay(Id);
        }
        PendingDismissals.clear();

        const bool PressedOutside = Input.MouseButtonPressedThisFrame[0] && !Input.PointerOccluded &&
            !PointInRect(Input.MousePosition, TopPlacement);
        if (!ContentConsumed && !Overlays.empty() && TopDismissesOutside && PressedOutside) {
            DismissOverlay(TopId);
        }
    }

    // Root, beneath the overlays, still updates, so a hover there clears and its
    // animations run, but with the pointer occluded and no keys: an open overlay has
    // the keyboard too.
    if (Root) {
        Platform::InputState Beneath = Input;
        Beneath.PointerOccluded = true;
        Beneath.TextInputThisFrame.clear();
        Beneath.KeysPressedThisFrame.clear();
        Beneath.KeyEvents.clear();
        Root->UpdateInteractionState(Beneath);
    }

    // Opaque even over its own dead space (padding, gaps between overlay content)
    // so a click meant for the popup layer never falls through to Root or a
    // lower overlay just because the content itself didn't claim it.
    return true;
}

void OverlayHost::Draw(Render::Renderer& Renderer) {
    // Root's Draw fully balances any clip/transform/blend pushes it makes before
    // returning, so every overlay drawn after it starts from a clean stack —
    // this is what lets overlay content escape Root's clip rect without the
    // Renderer needing a dedicated "layer" concept.
    if (Root) {
        Root->Draw(Renderer);
    }
    for (Overlay& Entry : Overlays) {
        Entry.Content->Draw(Renderer);
    }
}

std::size_t OverlayHost::GetChildCount() const {
    return (Root ? 1u : 0u) + Overlays.size();
}

WidgetBase* OverlayHost::GetChildAt(std::size_t Index) const {
    if (Root) {
        if (Index == 0) {
            return Root.get();
        }
        --Index;
    }
    if (Index < Overlays.size()) {
        return Overlays[Index].Content.get();
    }
    return nullptr;
}

} // namespace Penumbra::Widgets
