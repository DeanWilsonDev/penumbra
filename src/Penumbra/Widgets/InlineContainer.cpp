#include "Penumbra/Widgets/InlineContainer.h"

#include <algorithm>
#include <utility>

namespace Penumbra::Widgets {

namespace {
float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }
} // namespace

std::vector<std::vector<InlineContainer::LineItem>>
InlineContainer::BuildLines(Point ContentAvailable) const {
    std::vector<std::vector<LineItem>> Lines;
    std::vector<LineItem>              CurrentLine;
    float                              CursorX = 0.0f;

    for (const auto& ChildOwner : Children) {
        WidgetBase* Child = ChildOwner.get();
        if (!Child->GetIsVisible()) {
            continue; // absent, not zero-sized-but-present, same rule Box uses
        }

        const EdgeInsets Margin = Child->GetMarginLogical();
        const Point ChildAvailable{NonNegative(ContentAvailable.X - Margin.Left - Margin.Right),
                                   NonNegative(ContentAvailable.Y - Margin.Top - Margin.Bottom)};
        Point Desired = Child->Measure(ChildAvailable);
        if (ChildAvailable.X > 0.0f) {
            Desired.X = std::min(Desired.X, ChildAvailable.X);
        }
        const float ItemWidth = Margin.Left + Desired.X + Margin.Right;

        if (!CurrentLine.empty() && CursorX + ChildGap + ItemWidth > ContentAvailable.X) {
            Lines.push_back(std::move(CurrentLine));
            CurrentLine.clear();
            CursorX = 0.0f;
        }

        CursorX += (CurrentLine.empty() ? 0.0f : ChildGap) + ItemWidth;
        CurrentLine.push_back({Child, Desired, Margin});
    }

    if (!CurrentLine.empty()) {
        Lines.push_back(std::move(CurrentLine));
    }
    return Lines;
}

Point InlineContainer::Measure(Point AvailableSizeLogical) {
    SyncLayoutTransitions();
    const Point Frame = FrameSize();
    const Point ContentAvailable{NonNegative(AvailableSizeLogical.X - Frame.X),
                                 NonNegative(AvailableSizeLogical.Y - Frame.Y)};

    const auto Lines = BuildLines(ContentAvailable);

    float TotalHeight  = 0.0f;
    float MaxLineWidth = 0.0f;
    for (std::size_t LineIndex = 0; LineIndex < Lines.size(); ++LineIndex) {
        float LineWidth  = 0.0f;
        float LineHeight = 0.0f;
        for (std::size_t ItemIndex = 0; ItemIndex < Lines[LineIndex].size(); ++ItemIndex) {
            const LineItem& Item = Lines[LineIndex][ItemIndex];
            if (ItemIndex > 0) {
                LineWidth += ChildGap;
            }
            LineWidth += Item.Margin.Left + Item.Desired.X + Item.Margin.Right;
            LineHeight = std::max(LineHeight, Item.Margin.Top + Item.Desired.Y + Item.Margin.Bottom);
        }

        if (LineIndex > 0) {
            TotalHeight += LineGapLogical;
        }
        TotalHeight += LineHeight;
        MaxLineWidth = std::max(MaxLineWidth, LineWidth);
    }

    return {MaxLineWidth + Frame.X, TotalHeight + Frame.Y};
}

void InlineContainer::Arrange(Rect FinalRectLogical) {
    ArrangedRect = FinalRectLogical;

    const Rect Content = ContentRectFrom(FinalRectLogical);
    const auto Lines    = BuildLines({Content.W, Content.H});

    // Places a child within its line's cross extent (vertical, here), the same
    // shape as Box::Arrange's own PlaceCross for its stacks.
    auto PlaceCross = [&](float LineTop, float LineHeight, float ChildExtent, float MarginTop,
                          float MarginBottom) -> std::pair<float, float> {
        const float Available = NonNegative(LineHeight - MarginTop - MarginBottom);
        switch (CrossAlignment) {
        case CrossAlign::Start:
            return {LineTop + MarginTop, ChildExtent};
        case CrossAlign::Center:
            return {LineTop + MarginTop + (Available - ChildExtent) / 2.0f, ChildExtent};
        case CrossAlign::End:
            return {LineTop + LineHeight - MarginBottom - ChildExtent, ChildExtent};
        case CrossAlign::Stretch:
            return {LineTop + MarginTop, Available};
        }
        return {LineTop + MarginTop, ChildExtent};
    };

    float CursorY = Content.Y;
    for (std::size_t LineIndex = 0; LineIndex < Lines.size(); ++LineIndex) {
        const std::vector<LineItem>& Line = Lines[LineIndex];

        float LineHeight = 0.0f;
        for (const LineItem& Item : Line) {
            LineHeight = std::max(LineHeight, Item.Margin.Top + Item.Desired.Y + Item.Margin.Bottom);
        }

        float CursorX = Content.X;
        for (std::size_t ItemIndex = 0; ItemIndex < Line.size(); ++ItemIndex) {
            const LineItem& Item = Line[ItemIndex];
            if (ItemIndex > 0) {
                CursorX += ChildGap;
            }
            const float Left = CursorX + Item.Margin.Left;
            auto [Top, Height] =
                PlaceCross(CursorY, LineHeight, Item.Desired.Y, Item.Margin.Top, Item.Margin.Bottom);
            Item.Child->Arrange({Left, Top, Item.Desired.X, Height});
            CursorX = Left + Item.Desired.X + Item.Margin.Right;
        }

        CursorY += LineHeight;
        if (LineIndex + 1 < Lines.size()) {
            CursorY += LineGapLogical;
        }
    }
}

InlineContainer::Builder::Builder() : Owned(std::make_unique<InlineContainer>()) {}

InlineContainer::Builder& InlineContainer::Builder::className(std::string Value) {
    Owned->ClassName = std::move(Value);
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::child(std::unique_ptr<WidgetBase> Child) {
    Owned->AddChild(std::move(Child));
    return *this;
}

InlineContainer::Builder&
InlineContainer::Builder::children(std::vector<std::unique_ptr<WidgetBase>> Kids) {
    for (auto& Kid : Kids) {
        Owned->AddChild(std::move(Kid));
    }
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::onPress(std::function<void()> Handler) {
    Owned->OnPressed = std::move(Handler);
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::onRelease(std::function<void()> Handler) {
    Owned->OnReleased = std::move(Handler);
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::onHover(std::function<void()> Handler) {
    Owned->OnHovered = std::move(Handler);
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::onFocus(std::function<void()> Handler) {
    Owned->OnFocused = std::move(Handler);
    return *this;
}

InlineContainer::Builder& InlineContainer::Builder::onChange(std::function<void()> Handler) {
    Owned->OnChanged = std::move(Handler);
    return *this;
}

std::unique_ptr<InlineContainer> InlineContainer::Builder::build() { return std::move(Owned); }

} // namespace Penumbra::Widgets
