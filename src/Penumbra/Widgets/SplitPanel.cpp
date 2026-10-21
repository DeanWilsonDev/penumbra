#include "Penumbra/Widgets/SplitPanel.h"

#include "Penumbra/Widgets/Positioning.h"
#include "Penumbra/Widgets/Stacking.h"

#include <algorithm>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;

float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }

// Insets a rect by a child's margin, same shape as Box::Arrange's margin handling
// for stack children.
Rect ApplyMargin(Rect Outer, EdgeInsets Margin) {
    return {Outer.X + Margin.Left, Outer.Y + Margin.Top,
            NonNegative(Outer.W - Margin.Left - Margin.Right),
            NonNegative(Outer.H - Margin.Top - Margin.Bottom)};
}
} // namespace

WidgetBase* SplitPanel::SetFirst(std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    First = std::move(Child);
    return Raw;
}

WidgetBase* SplitPanel::SetSecond(std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    Second = std::move(Child);
    return Raw;
}

void SplitPanel::ApplyStyle(const SplitPanelStyle& InStyle) {
    Style = static_cast<const BoxStyle&>(InStyle);
    ColorHandle        = InStyle.ColorHandle;
    ColorHandleHovered = InStyle.ColorHandleHovered;
    ColorHandleDragged = InStyle.ColorHandleDragged;
}

Point SplitPanel::Measure(Point AvailableSizeLogical) {
    // A split is greedy: like ViewportWidget, its bounds come from the parent's
    // layout, not from its children's intrinsic size.
    return AvailableSizeLogical;
}

void SplitPanel::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;
    const ContainingBlockScope Block(*this, PaddingRectFrom(FinalRectLogical));

    const bool  Vertical   = (Axis == SplitAxis::Vertical);
    const float MainExtent = Vertical ? FinalRectLogical.H : FinalRectLogical.W;
    const float Available  = std::max(0.0f, MainExtent - HandleThicknessLogical);

    // Clamp SplitRatio so neither pane shrinks below MinPaneSizeLogical.
    float MinRatio = 0.0f;
    float MaxRatio = 1.0f;
    if (Available > 0.0f) {
        MinRatio = std::clamp(MinPaneSizeLogical / Available, 0.0f, 1.0f);
        MaxRatio = std::clamp(1.0f - MinPaneSizeLogical / Available, 0.0f, 1.0f);
    }
    if (MinRatio > MaxRatio) {
        MinRatio = MaxRatio = 0.5f; // too small for both minimums — split evenly rather than favour a side
    }
    SplitRatio = std::clamp(SplitRatio, MinRatio, MaxRatio);

    const float FirstMain  = Available * SplitRatio;
    const float SecondMain = Available - FirstMain;

    Rect FirstRect;
    Rect SecondRect;
    if (Vertical) {
        FirstRect  = {FinalRectLogical.X, FinalRectLogical.Y, FinalRectLogical.W, FirstMain};
        HandleRect = {FinalRectLogical.X, FinalRectLogical.Y + FirstMain, FinalRectLogical.W,
                      HandleThicknessLogical};
        SecondRect = {FinalRectLogical.X, FinalRectLogical.Y + FirstMain + HandleThicknessLogical,
                      FinalRectLogical.W, SecondMain};
    } else {
        FirstRect  = {FinalRectLogical.X, FinalRectLogical.Y, FirstMain, FinalRectLogical.H};
        HandleRect = {FinalRectLogical.X + FirstMain, FinalRectLogical.Y, HandleThicknessLogical,
                      FinalRectLogical.H};
        SecondRect = {FinalRectLogical.X + FirstMain + HandleThicknessLogical, FinalRectLogical.Y,
                      SecondMain, FinalRectLogical.H};
    }

    if (First) {
        const Rect Inset = ApplyMargin(FirstRect, First->GetMarginLogical());
        First->Measure({Inset.W, Inset.H});
        First->Arrange(Inset);
    }
    if (Second) {
        const Rect Inset = ApplyMargin(SecondRect, Second->GetMarginLogical());
        Second->Measure({Inset.W, Inset.H});
        Second->Arrange(Inset);
    }
}

bool SplitPanel::UpdateInteractionState(const Platform::InputState& Given) {
    if (StackingContext::IsLayeredOut(*this)) {
        return false; // its stacking context hands it input itself, in paint order
    }
    Platform::InputState        PointerEventsInput;
    const Platform::InputState& Input = InputForPointerEvents(Style.PointerEventsMode, Given, PointerEventsInput);

    // Reverse paint order: layered descendants first, over the handle and the panes.
    StackingContext             Context(*this, StackingContext::Pass::Input);
    Platform::OccludedInput     Occluded(Input);
    const bool                  LayeredTook = Context.DispatchAbove(Input);
    const Platform::InputState& Unlayered   = LayeredTook ? Occluded.Get() : Input;

    const bool Hovered  = Unlayered.IsPointerOver(HandleRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

    const bool  Vertical    = (Axis == SplitAxis::Vertical);
    const float PointerMain = Vertical ? Input.MousePosition.Y : Input.MousePosition.X;

    if (Pressed && Hovered) {
        Dragging = true;
        LastPointerMain = PointerMain;
    }
    if (Dragging && Down) {
        const float MainExtent = Vertical ? ArrangedRect.H : ArrangedRect.W;
        const float Delta = PointerMain - LastPointerMain;
        if (Delta != 0.0f && MainExtent > 0.0f) {
            SplitRatio = std::clamp(SplitRatio + Delta / MainExtent, 0.0f, 1.0f);
            LastPointerMain = PointerMain;
        }
    }
    if (Released) {
        Dragging = false;
    }
    HandleHovered = Hovered;

    // Reverse order, same first-refusal rule as Box: Second is drawn on top of First,
    // and First still updates, with the pointer occluded, when Second takes it.
    bool PaneConsumed = LayeredTook;
    for (WidgetBase* Pane : {Second.get(), First.get()}) {
        Platform::InputState PaneInput;
        if (Pane && !StackingContext::IsLayeredOut(*Pane) &&
            Pane->UpdateInteractionState(InputForChild(*Pane, PaneConsumed ? Occluded.Get() : Input, PaneInput))) {
            PaneConsumed = true;
        }
    }
    if (Context.DispatchBelow(Input, PaneConsumed || Hovered)) {
        PaneConsumed = true;
    }

    return PaneConsumed || Dragging || Hovered;
}

std::size_t SplitPanel::GetChildCount() const { return (First ? 1u : 0u) + (Second ? 1u : 0u); }

WidgetBase* SplitPanel::GetChildAt(std::size_t Index) const {
    if (First) {
        if (Index == 0) {
            return First.get();
        }
        --Index;
    }
    if (Second && Index == 0) {
        return Second.get();
    }
    return nullptr;
}

void SplitPanel::Draw(Render::Renderer& Renderer) {
    if (StackingContext::IsLayeredOut(*this)) {
        return; // its stacking context paints it later, in z order
    }
    StackingContext Context(*this, StackingContext::Pass::Paint);
    Context.PaintBelow(Renderer);
    for (WidgetBase* Pane : {First.get(), Second.get()}) {
        if (Pane && !StackingContext::IsLayeredOut(*Pane)) {
            Pane->Draw(Renderer);
        }
    }

    const Render::Color HandleColor =
        Dragging ? ColorHandleDragged : (HandleHovered ? ColorHandleHovered : ColorHandle);
    if (HandleColor.A != 0) {
        Renderer.DrawFilledRect(HandleRect, HandleColor);
    }
    Context.PaintAbove(Renderer);
}

} // namespace Penumbra::Widgets
