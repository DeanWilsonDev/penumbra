#include "Penumbra/Widgets/Positioning.h"

#include "Penumbra/Widgets/Length.h"

#include <optional>
#include <vector>

namespace Penumbra::Widgets {

namespace {

// The containing blocks of the widgets being arranged right now, innermost last.
struct Block {
    Rect PaddingBox;
    bool ForFixed{false};
};
std::vector<Block> Blocks;

float NonNegative(float Value) { return Value > 0.0f ? Value : 0.0f; }

std::optional<float> Resolve(const std::optional<Length>& Offset, float BasisLogical) {
    return Offset ? std::optional<float>(Offset->Resolve(BasisLogical)) : std::nullopt;
}

Rect ContainingBlockFor(bool Fixed) {
    for (auto Iterator = Blocks.rbegin(); Iterator != Blocks.rend(); ++Iterator) {
        if (!Fixed || Iterator->ForFixed) {
            return Iterator->PaddingBox;
        }
    }
    const Point Viewport = GetLayoutViewportLogical();
    return {0.0f, 0.0f, Viewport.X, Viewport.Y};
}

} // namespace

bool IsOutOfFlow(const WidgetBase& Child) {
    const BoxStyle* Style = Child.GetBoxStyle();
    return Style && Child.GetIsVisible() &&
           (Style->PositionMode == Position::Absolute || Style->PositionMode == Position::Fixed);
}

Rect OffsetInFlow(const WidgetBase& Child, Rect InFlowRect, Rect ParentContent) {
    const BoxStyle* Style = Child.GetBoxStyle();
    if (!Style || Style->PositionMode != Position::Relative) {
        return InFlowRect;
    }
    if (const std::optional<float> Left = Resolve(Style->Left, ParentContent.W)) {
        InFlowRect.X += *Left;
    } else if (const std::optional<float> Right = Resolve(Style->Right, ParentContent.W)) {
        InFlowRect.X -= *Right;
    }
    if (const std::optional<float> Top = Resolve(Style->Top, ParentContent.H)) {
        InFlowRect.Y += *Top;
    } else if (const std::optional<float> Bottom = Resolve(Style->Bottom, ParentContent.H)) {
        InFlowRect.Y -= *Bottom;
    }
    return InFlowRect;
}

void ArrangeOutOfFlow(WidgetBase& Child, Point StaticPosition) {
    const BoxStyle* Style = Child.GetBoxStyle();
    if (!Style) {
        return;
    }
    const Rect       Block  = ContainingBlockFor(Style->PositionMode == Position::Fixed);
    const EdgeInsets Margin = Child.GetMarginLogical();

    const std::optional<float> Left   = Resolve(Style->Left, Block.W);
    const std::optional<float> Right  = Resolve(Style->Right, Block.W);
    const std::optional<float> Top    = Resolve(Style->Top, Block.H);
    const std::optional<float> Bottom = Resolve(Style->Bottom, Block.H);

    // With both offsets of a pair and no size of its own, it stretches between them. A
    // size of its own resolves against the whole containing block; without one, it
    // shrinks to fit the space the offsets leave, as CSS sizes an auto width.
    const bool  StretchX = Left && Right && !Style->Width.IsSet();
    const bool  StretchY = Top && Bottom && !Style->Height.IsSet();
    const float SpaceX   = NonNegative(Block.W - Margin.Left - Margin.Right);
    const float SpaceY   = NonNegative(Block.H - Margin.Top - Margin.Bottom);
    const float AvailableX =
        Style->Width.IsSet() && !StretchX ? SpaceX : NonNegative(SpaceX - Left.value_or(0.0f) - Right.value_or(0.0f));
    const float AvailableY = Style->Height.IsSet() && !StretchY
                                 ? SpaceY
                                 : NonNegative(SpaceY - Top.value_or(0.0f) - Bottom.value_or(0.0f));

    const Point Desired = Child.Measure({AvailableX, AvailableY});
    const float Width   = StretchX ? AvailableX : Desired.X;
    const float Height  = StretchY ? AvailableY : Desired.Y;

    float X = StaticPosition.X;
    if (Left) {
        X = Block.X + *Left + Margin.Left;
    } else if (Right) {
        X = Block.X + Block.W - *Right - Margin.Right - Width;
    }
    float Y = StaticPosition.Y;
    if (Top) {
        Y = Block.Y + *Top + Margin.Top;
    } else if (Bottom) {
        Y = Block.Y + Block.H - *Bottom - Margin.Bottom - Height;
    }
    Child.Arrange({X, Y, Width, Height});
}

ContainingBlockScope::ContainingBlockScope(const WidgetBase& Widget, Rect PaddingBox) {
    if (Blocks.empty()) {
        // The initial containing block: the window, when the host has said how big it
        // is (Application does, through SetLayoutViewportLogical), else the outermost
        // widget arranged.
        const Point Viewport = GetLayoutViewportLogical();
        const bool  Known    = Viewport.X > 0.0f && Viewport.Y > 0.0f;
        Blocks.push_back({Known ? Rect{0.0f, 0.0f, Viewport.X, Viewport.Y} : PaddingBox, true});
        ++Pushed;
    }
    const BoxStyle* Style       = Widget.GetBoxStyle();
    const bool      Positioned  = Style && Style->PositionMode != Position::Static;
    const bool      Transformed = Style && !Style->Transform.IsIdentity();
    if (Positioned || Transformed) {
        Blocks.push_back({PaddingBox, Transformed});
        ++Pushed;
    }
}

ContainingBlockScope::~ContainingBlockScope() {
    Blocks.resize(Blocks.size() - Pushed);
}

} // namespace Penumbra::Widgets
