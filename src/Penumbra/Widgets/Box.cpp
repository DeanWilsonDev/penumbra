#include "Penumbra/Widgets/Box.h"

#include <algorithm>
#include <utility>

namespace Penumbra::Widgets {

namespace {
float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }
} // namespace

WidgetBase* Box::AddChild(std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    Children.push_back(std::move(Child));
    return Raw;
}

WidgetBase* Box::InsertChildAt(std::size_t Index, std::unique_ptr<WidgetBase> Child) {
    WidgetBase* Raw = Child.get();
    const std::size_t Clamped = std::min(Index, Children.size());
    Children.insert(Children.begin() + static_cast<std::ptrdiff_t>(Clamped), std::move(Child));
    return Raw;
}

void Box::RemoveChild(WidgetBase* Child) {
    auto Iterator = std::find_if(Children.begin(), Children.end(),
                                  [Child](const std::unique_ptr<WidgetBase>& Owned) { return Owned.get() == Child; });
    if (Iterator != Children.end()) {
        Children.erase(Iterator);
    }
}

std::unique_ptr<WidgetBase> Box::ReplaceChild(WidgetBase* Existing, std::unique_ptr<WidgetBase> Replacement) {
    auto Iterator = std::find_if(
        Children.begin(), Children.end(),
        [Existing](const std::unique_ptr<WidgetBase>& Owned) { return Owned.get() == Existing; });
    if (Iterator == Children.end()) {
        return nullptr;
    }
    std::unique_ptr<WidgetBase> Removed = std::move(*Iterator);
    *Iterator                           = std::move(Replacement);
    return Removed;
}

void Box::ClearChildren() { Children.clear(); }

void Box::MoveChild(std::size_t FromIndex, std::size_t ToIndex) {
    if (FromIndex == ToIndex || FromIndex >= Children.size() || ToIndex >= Children.size()) {
        return;
    }
    auto Begin = Children.begin();
    if (FromIndex < ToIndex) {
        std::rotate(Begin + static_cast<std::ptrdiff_t>(FromIndex), Begin + static_cast<std::ptrdiff_t>(FromIndex) + 1,
                    Begin + static_cast<std::ptrdiff_t>(ToIndex) + 1);
    } else {
        std::rotate(Begin + static_cast<std::ptrdiff_t>(ToIndex), Begin + static_cast<std::ptrdiff_t>(FromIndex),
                    Begin + static_cast<std::ptrdiff_t>(FromIndex) + 1);
    }
}

Point Box::FrameSize() const {
    const float Horizontal = Style.Padding.Left + Style.Padding.Right + 2.0f * Style.BorderWidth;
    const float Vertical   = Style.Padding.Top + Style.Padding.Bottom + 2.0f * Style.BorderWidth;
    return {Horizontal, Vertical};
}

Rect Box::ContentRectFrom(Rect OuterRect) const {
    const float B = Style.BorderWidth;
    return {OuterRect.X + B + Style.Padding.Left,
            OuterRect.Y + B + Style.Padding.Top,
            NonNegative(OuterRect.W - 2.0f * B - Style.Padding.Left - Style.Padding.Right),
            NonNegative(OuterRect.H - 2.0f * B - Style.Padding.Top - Style.Padding.Bottom)};
}

Point Box::Measure(Point AvailableSizeLogical) {
    const Point Frame = FrameSize();
    const Point ContentAvailable{NonNegative(AvailableSizeLogical.X - Frame.X),
                                 NonNegative(AvailableSizeLogical.Y - Frame.Y)};

    Point ContentDesired{0.0f, 0.0f};

    if (Layout == LayoutMode::None) {
        ContentDesired = MeasureContent(ContentAvailable);
    } else {
        const bool Vertical = (Layout == LayoutMode::VerticalStack);
        float MainTotal = 0.0f;
        float CrossMax  = 0.0f;
        bool  AnyVisible = false;

        for (std::size_t Index = 0; Index < Children.size(); ++Index) {
            WidgetBase* Child = Children[Index].get();
            if (!Child->GetIsVisible()) {
                continue; // absent, not zero-sized-but-present: no gap counted either
            }
            const EdgeInsets Margin = Child->GetMarginLogical();

            const Point ChildAvailable{NonNegative(ContentAvailable.X - Margin.Left - Margin.Right),
                                       NonNegative(ContentAvailable.Y - Margin.Top - Margin.Bottom)};
            const Point Desired = Child->Measure(ChildAvailable);

            const float ChildMain   = Vertical ? Desired.Y : Desired.X;
            const float ChildCross  = Vertical ? Desired.X : Desired.Y;
            const float MarginMain  = Vertical ? (Margin.Top + Margin.Bottom) : (Margin.Left + Margin.Right);
            const float MarginCross = Vertical ? (Margin.Left + Margin.Right) : (Margin.Top + Margin.Bottom);

            if (AnyVisible) {
                MainTotal += ChildGap;
            }
            MainTotal += ChildMain + MarginMain;
            CrossMax = std::max(CrossMax, ChildCross + MarginCross);
            AnyVisible = true;
        }

        ContentDesired = Vertical ? Point{CrossMax, MainTotal} : Point{MainTotal, CrossMax};
    }

    return {ContentDesired.X + Frame.X, ContentDesired.Y + Frame.Y};
}

void Box::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;

    if (Layout == LayoutMode::None) {
        return; // children, if any, are not laid out (footgun accepted)
    }

    const Rect Content = ContentRectFrom(FinalRectLogical);
    const bool Vertical = (Layout == LayoutMode::VerticalStack);

    // Places a child along the CROSS axis, returning its {position, extent}.
    auto PlaceCross = [&](float ContentStart, float ContentExtent, float ChildExtent,
                          float MarginLead, float MarginTrail) -> std::pair<float, float> {
        const float Available = NonNegative(ContentExtent - MarginLead - MarginTrail);
        switch (CrossAlignment) {
        case CrossAlign::Start:
            return {ContentStart + MarginLead, ChildExtent};
        case CrossAlign::Center:
            return {ContentStart + MarginLead + (Available - ChildExtent) / 2.0f, ChildExtent};
        case CrossAlign::End:
            return {ContentStart + ContentExtent - MarginTrail - ChildExtent, ChildExtent};
        case CrossAlign::Stretch:
            return {ContentStart + MarginLead, Available};
        }
        return {ContentStart + MarginLead, ChildExtent};
    };

    const std::size_t Count = Children.size();
    float Cursor = Vertical ? Content.Y : Content.X;
    bool  AnyVisible = false;

    for (std::size_t Index = 0; Index < Count; ++Index) {
        WidgetBase* Child = Children[Index].get();
        if (!Child->GetIsVisible()) {
            continue; // not arranged: an un-arranged widget keeps its last ArrangedRect
        }
        const EdgeInsets Margin = Child->GetMarginLogical();

        const Point ChildAvailable{NonNegative(Content.W - Margin.Left - Margin.Right),
                                   NonNegative(Content.H - Margin.Top - Margin.Bottom)};
        const Point Desired = Child->Measure(ChildAvailable);

        if (AnyVisible) {
            Cursor += ChildGap;
        }

        if (Vertical) {
            auto [CrossPos, CrossExtent] =
                PlaceCross(Content.X, Content.W, Desired.X, Margin.Left, Margin.Right);
            const float Top = Cursor + Margin.Top;
            Child->Arrange({CrossPos, Top, CrossExtent, Desired.Y});
            Cursor = Top + Desired.Y + Margin.Bottom;
        } else {
            auto [CrossPos, CrossExtent] =
                PlaceCross(Content.Y, Content.H, Desired.Y, Margin.Top, Margin.Bottom);
            const float Left = Cursor + Margin.Left;
            Child->Arrange({Left, CrossPos, Desired.X, CrossExtent});
            Cursor = Left + Desired.X + Margin.Right;
        }

        AnyVisible = true;
    }
}

bool Box::UpdateInteractionState(const Platform::InputState& Input) {
    // Depth-first, last child (drawn on top) gets first refusal.
    for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator) {
        if (!(*Iterator)->GetIsVisible()) {
            continue; // a hidden subtree can't consume input
        }
        if ((*Iterator)->UpdateInteractionState(Input)) {
            return true;
        }
    }
    return false;
}

void Box::Draw(Render::Renderer& Renderer) {
    if (Style.ColorBackground.A != 0) {
        Renderer.DrawFilledRect(ArrangedRect, Style.ColorBackground, Style.BorderRadius);
    }
    if (Style.BorderWidth > 0.0f && Style.ColorBorder.A != 0) {
        Renderer.DrawRectOutline(ArrangedRect, Style.ColorBorder, Style.BorderWidth, Style.BorderRadius);
    }

    DrawContent(Renderer, ContentRectFrom(ArrangedRect));

    for (auto& Child : Children) {
        if (!Child->GetIsVisible()) {
            continue;
        }
        Child->Draw(Renderer);
    }
}

} // namespace Penumbra::Widgets
