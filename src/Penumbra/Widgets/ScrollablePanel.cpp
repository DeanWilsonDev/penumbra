#include "Penumbra/Widgets/ScrollablePanel.h"

#include <algorithm>
#include <utility>

namespace Penumbra::Widgets {

namespace {

float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }

float Clamp(float Value, float Low, float High) {
    return std::min(std::max(Value, Low), High);
}

} // namespace

Point ScrollablePanel::Measure(Point AvailableSizeLogical) {
    const Point Frame = FrameSize();
    const Point Viewport = ConstrainAvailable(AvailableSizeLogical);
    const Point ContentAvailable{NonNegative(Viewport.X - Frame.X), NonNegative(Viewport.Y - Frame.Y)};

    float Total = 0.0f;
    float MaxWidth = 0.0f;
    bool  AnyVisible = false;
    for (std::size_t Index = 0; Index < Children.size(); ++Index) {
        WidgetBase* Child = Children[Index].get();
        if (!Child->GetIsVisible()) {
            continue;
        }
        const EdgeInsets Margin = Child->GetMarginLogical();
        const Point ChildAvailable{NonNegative(ContentAvailable.X - Margin.Left - Margin.Right),
                                   NonNegative(ContentAvailable.Y - Margin.Top - Margin.Bottom)};
        const Point Desired = Child->Measure(ChildAvailable);

        if (AnyVisible) {
            Total += ChildGap;
        }
        Total += Desired.Y + Margin.Top + Margin.Bottom;
        MaxWidth = std::max(MaxWidth, Desired.X + Margin.Left + Margin.Right);
        AnyVisible = true;
    }

    ContentHeight = Total;
    ContentWidth = MaxWidth;

    if (Direction == ScrollDirection::Horizontal) {
        return ConstrainDesired({Viewport.X, Total + Frame.Y}, AvailableSizeLogical);
    }
    return ConstrainDesired({MaxWidth + Frame.X, Viewport.Y}, AvailableSizeLogical);
}

void ScrollablePanel::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;

    const Rect Content = ContentRectFrom(FinalRectLogical);
    const bool IsHorizontal = (Direction == ScrollDirection::Horizontal);
    if (IsHorizontal) {
        // No vertical scroll at all in this mode -- ContentHeight was sized to exactly
        // Content.H by Measure above, so this clamps to 0 regardless, but stating it
        // directly is clearer than relying on that coincidence.
        ScrollOffsetY = 0.0f;
        const float MaxScrollX = NonNegative(ContentWidth - Content.W);
        ScrollOffsetX = Clamp(ScrollOffsetX, 0.0f, MaxScrollX);
    } else {
        const float MaxScroll = NonNegative(ContentHeight - Content.H);
        ScrollOffsetY = Clamp(ScrollOffsetY, 0.0f, MaxScroll);
    }

    // Lay children out as a vertical column, shifted up by the (vertical-mode) scroll
    // offset. Positions may fall outside the viewport; the clip in Draw hides them.
    float CursorY = Content.Y - ScrollOffsetY;
    const std::size_t Count = Children.size();
    bool  AnyVisible = false;
    for (std::size_t Index = 0; Index < Count; ++Index) {
        WidgetBase* Child = Children[Index].get();
        if (!Child->GetIsVisible()) {
            continue; // not arranged: an un-arranged widget keeps its last ArrangedRect --
                      // same contract Box::Arrange already gives its own children.
        }
        const EdgeInsets Margin = Child->GetMarginLogical();

        const Point ChildAvailable{NonNegative(Content.W - Margin.Left - Margin.Right),
                                   NonNegative(Content.H - Margin.Top - Margin.Bottom)};
        const Point Desired = Child->Measure(ChildAvailable);

        const float AvailableCross = NonNegative(Content.W - Margin.Left - Margin.Right);
        float ChildX = Content.X + Margin.Left;
        float ChildWidth = Desired.X;
        if (IsHorizontal) {
            // The cross axis is the scrolled viewport now -- children are positioned
            // from Start and shifted by the horizontal scroll offset, same shape as
            // CursorY's own "- ScrollOffsetY" above; CrossAlignment (Center/End/
            // Stretch) has no sensible meaning once a child is allowed to exceed the
            // viewport width, so it's not consulted in this mode.
            ChildX = Content.X + Margin.Left - ScrollOffsetX;
        } else {
            switch (CrossAlignment) {
            case CrossAlign::Start:
                break;
            case CrossAlign::Center:
                ChildX = Content.X + Margin.Left + (AvailableCross - Desired.X) / 2.0f;
                break;
            case CrossAlign::End:
                ChildX = Content.X + Content.W - Margin.Right - Desired.X;
                break;
            case CrossAlign::Stretch:
                ChildWidth = AvailableCross;
                break;
            }
        }

        if (AnyVisible) {
            CursorY += ChildGap;
        }

        const float ChildY = CursorY + Margin.Top;
        Child->Arrange({ChildX, ChildY, ChildWidth, Desired.Y});

        CursorY = ChildY + Desired.Y + Margin.Bottom;
        AnyVisible = true;
    }
}

bool ScrollablePanel::UpdateInteractionState(const Platform::InputState& Input) {
    const bool OverPanel = Input.IsPointerOver(ArrangedRect);
    PointerOver = OverPanel;

    // Children see the pointer only while it's over the visible content, and only
    // until one of them takes it; the rest still update, with it occluded.
    const Rect              Content = ContentRectFrom(ArrangedRect);
    Platform::OccludedInput Occluded(Input);
    bool                    PointerTaken       = !Input.IsPointerOver(Content);
    bool                    Consumed           = false;
    bool                    ChildConsumedWheel = false;
    for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator) {
        if (!(*Iterator)->GetIsVisible()) {
            continue;
        }
        if ((*Iterator)->UpdateInteractionState(PointerTaken ? Occluded.Get() : Input) && !PointerTaken) {
            PointerTaken       = true;
            Consumed           = true;
            ChildConsumedWheel = (*Iterator)->ConsumedWheelThisFrame();
        }
    }

    WheelHandledThisFrame = false;
    if (!OverPanel) {
        return false;
    }

    // A nested wheel-scrollable child (another ScrollablePanel, or a TextArea) that
    // still had room to scroll already used the wheel delta on itself -- don't also
    // apply it here, or hovering it would scroll both it and this panel at once.
    if (Direction == ScrollDirection::Horizontal) {
        if (!ChildConsumedWheel && HorizontalWheelStepLogical != 0.0f && Input.MouseWheelDeltaX != 0.0f) {
            const float MaxScrollX = NonNegative(ContentWidth - Content.W);
            if (MaxScrollX > 0.0f) {
                ScrollOffsetX =
                    Clamp(ScrollOffsetX - Input.MouseWheelDeltaX * HorizontalWheelStepLogical, 0.0f, MaxScrollX);
                Consumed = true;
                WheelHandledThisFrame = true;
            }
        }
    } else if (!ChildConsumedWheel && WheelStepLogical != 0.0f && Input.MouseWheelDelta != 0.0f) {
        const float MaxScroll = NonNegative(ContentHeight - Content.H);
        if (MaxScroll > 0.0f) {
            ScrollOffsetY = Clamp(ScrollOffsetY - Input.MouseWheelDelta * WheelStepLogical, 0.0f, MaxScroll);
            Consumed = true;
            WheelHandledThisFrame = true;
        }
    }

    // The panel is opaque: being over it consumes input regardless.
    return Consumed || OverPanel;
}

void ScrollablePanel::Draw(Render::Renderer& Renderer) {
    if (Style.ColorBackground.A != 0) {
        Renderer.DrawFilledRect(ArrangedRect, Style.ColorBackground, Style.BorderRadius);
    }
    if (Style.BorderWidth > 0.0f && Style.ColorBorder.A != 0) {
        Renderer.DrawRectOutline(ArrangedRect, Style.ColorBorder, Style.BorderWidth, Style.BorderRadius);
    }

    const Rect Content = ContentRectFrom(ArrangedRect);
    Renderer.PushClipRect(Content);
    for (auto& Child : Children) {
        if (!Child->GetIsVisible()) {
            continue;
        }
        Child->Draw(Renderer);
    }
    Renderer.PopClipRect();

    DrawScrollbar(Renderer, Content);
}

void ScrollablePanel::DrawScrollbar(Render::Renderer& Renderer, Rect Content) const {
    if (ScrollbarWidthLogical <= 0.0f) {
        return;
    }

    const bool  IsHorizontal = (Direction == ScrollDirection::Horizontal);
    const float Viewport     = IsHorizontal ? Content.W : Content.H;
    const float Extent       = IsHorizontal ? ContentWidth : ContentHeight;
    if (Viewport <= 0.0f || Extent <= Viewport) {
        return;
    }

    const float Thickness = ScrollbarWidthLogical;
    const Rect  Track = IsHorizontal
        ? Rect{Content.X, ArrangedRect.Y + ArrangedRect.H - Style.BorderWidth - Thickness, Content.W, Thickness}
        : Rect{ArrangedRect.X + ArrangedRect.W - Style.BorderWidth - Thickness, Content.Y, Thickness, Content.H};
    const float Radius = Thickness / 2.0f;

    if (ColorScrollbarTrack.A != 0) {
        Renderer.DrawFilledRect(Track, ColorScrollbarTrack, Radius);
    }

    const Render::Color Thumb =
        PointerOver && ColorScrollbarThumbHovered.A != 0 ? ColorScrollbarThumbHovered : ColorScrollbarThumb;
    if (Thumb.A == 0) {
        return;
    }

    const float TrackLength = IsHorizontal ? Track.W : Track.H;
    const float ThumbLength = std::min(TrackLength, std::max(Thickness, TrackLength * Viewport / Extent));
    const float Offset      = IsHorizontal ? ScrollOffsetX : ScrollOffsetY;
    const float ThumbStart  = (Offset / (Extent - Viewport)) * (TrackLength - ThumbLength);

    const Rect ThumbRect = IsHorizontal ? Rect{Track.X + ThumbStart, Track.Y, ThumbLength, Thickness}
                                        : Rect{Track.X, Track.Y + ThumbStart, Thickness, ThumbLength};
    Renderer.DrawFilledRect(ThumbRect, Thumb, Radius);
}

} // namespace Penumbra::Widgets
