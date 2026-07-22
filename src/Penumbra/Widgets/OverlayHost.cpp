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

OverlayId OverlayHost::ShowOverlay(std::unique_ptr<WidgetBase> Content, Rect PlacementRectLogical,
                                   bool DismissOnOutsideClick) {
    const OverlayId Id = NextId++;
    Overlays.push_back({Id, std::move(Content), PlacementRectLogical, DismissOnOutsideClick});
    return Id;
}

void OverlayHost::SetOverlayPlacement(OverlayId Id, Rect PlacementRectLogical) {
    for (Overlay& Entry : Overlays) {
        if (Entry.Id == Id) {
            Entry.PlacementRectLogical = PlacementRectLogical;
            return;
        }
    }
}

void OverlayHost::DismissOverlay(OverlayId Id) {
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
        Entry.Content->Measure({Entry.PlacementRectLogical.W, Entry.PlacementRectLogical.H});
        Entry.Content->Arrange(Entry.PlacementRectLogical);
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

    for (Platform::Key PressedKey : Input.KeysPressedThisFrame) {
        if (PressedKey == Platform::Key::Escape) {
            DismissOverlay(Top.Id);
            return true;
        }
    }

    if (Top.Content->UpdateInteractionState(Input)) {
        return true;
    }

    const bool PressedOutside = Input.MouseButtonPressedThisFrame[0] &&
        !PointInRect(Input.MousePosition, Top.PlacementRectLogical);
    if (Top.DismissOnOutsideClick && PressedOutside) {
        DismissOverlay(Top.Id);
        return true;
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
