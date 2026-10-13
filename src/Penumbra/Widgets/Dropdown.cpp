#include "Penumbra/Widgets/Dropdown.h"

#include "Penumbra/Anim/Animation.h"
#include "Penumbra/Widgets/OverlayHost.h"

#include <algorithm>
#include <utility>

namespace Penumbra::Widgets {

struct DropdownState {
    OverlayHost* Host{nullptr};
    OverlayId Id{0};
    bool IsOpen{false};
    Anim::Tween WidthTween;
    Rect TriggerRect{};
    float OpenWidth{0.0f};
    float Height{0.0f};
    float RowHeight{0.0f};
};

namespace {

Rect PanelRect(const DropdownState& State) {
    const float Progress = Anim::EaseOutCubic(State.WidthTween.Progress());
    const float Width = State.TriggerRect.W + (State.OpenWidth - State.TriggerRect.W) * Progress;
    const float Right = State.TriggerRect.X + State.TriggerRect.W;
    return {Right - Width, State.TriggerRect.Y, Width, State.Height};
}

class DropdownRow final : public Box {
public:
    DropdownItem Item;
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle Font{0};
    Render::Color TextColor{};
    Render::Color HighlightedTextColor{};
    float VisualSize{0.0f};
    float VisualGap{0.0f};
    bool AlwaysHighlighted{false};

protected:
    Point MeasureContent(Point) override { return {0.0f, Style.Height.Value}; }

    void DrawContent(Render::Renderer& Renderer, Rect ContentRect) override {
        const bool Highlighted = AlwaysHighlighted || GetInteractionState() != InteractionState::Default;
        const Render::Color Color = Highlighted ? HighlightedTextColor : TextColor;
        const Rect VisualRect{ContentRect.X, ContentRect.Y + (ContentRect.H - VisualSize) * 0.5f,
                              VisualSize, VisualSize};
        Renderer.PushClipRect(ContentRect);
        if (Item.DrawLeadingVisual) {
            Item.DrawLeadingVisual(Renderer, VisualRect, Color);
        }
        if (!FontBackend) {
            Renderer.PopClipRect();
            return;
        }
        const float TextX = ContentRect.X + VisualSize + VisualGap;
        const float TextHeight = FontBackend->MeasureText(Font, "Ag").HeightLogical;
        const float MaxWidth = std::max(0.0f, ContentRect.X + ContentRect.W - TextX);
        std::string Text = Item.Label;
        while (!Text.empty() && Renderer.MeasureTextWidth(Font, Text) > MaxWidth) {
            Text.pop_back();
        }
        if (Text.size() >= 2 && Text != Item.Label) {
            Text[Text.size() - 1] = '.';
            if (Text.size() >= 3) Text[Text.size() - 2] = '.';
        }
        Renderer.DrawText(Font, Text, {TextX, ContentRect.Y + (ContentRect.H - TextHeight) * 0.5f}, Color);
        Renderer.PopClipRect();
    }
};

class DropdownMenu final : public Box {
public:
    std::shared_ptr<DropdownState> State;

    bool UpdateInteractionState(const Platform::InputState& Input) override {
        State->WidthTween.Update(Input.DeltaTimeSeconds);
        State->Host->SetOverlayPlacement(State->Id, PanelRect(*State));
        return Box::UpdateInteractionState(Input);
    }
};

} // namespace

Dropdown::~Dropdown() { Dismiss(); }

void Dropdown::ApplyStyle(const DropdownStyle& StyleValue) {
    DropdownVisualStyle = StyleValue;
    Style = static_cast<const BoxStyle&>(StyleValue);
}

bool Dropdown::GetIsOpen() const { return State && State->IsOpen; }

void Dropdown::Dismiss() {
    if (State && State->IsOpen && State->Host) {
        State->Host->DismissOverlay(State->Id);
    }
}

Point Dropdown::MeasureContent(Point) {
    return {DropdownVisualStyle.TriggerContentSizeLogical, DropdownVisualStyle.TriggerContentSizeLogical};
}

void Dropdown::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (Items.empty() || SelectedIndex >= Items.size()) return;
    if (Items[SelectedIndex].DrawLeadingVisual) {
        Items[SelectedIndex].DrawLeadingVisual(Renderer, ContentRect, DropdownVisualStyle.ColorTextHighlighted);
    }
}

bool Dropdown::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Hovered = Input.IsPointerOver(ArrangedRect);
    if (Input.MouseButtonPressedThisFrame[0] && Hovered) PressedInside = true;
    if (Input.MouseButtonReleasedThisFrame[0]) {
        if (PressedInside && Hovered && !GetIsOpen()) Open();
        PressedInside = false;
    }
    return Box::UpdateInteractionState(Input);
}

void Dropdown::Open() {
    if (!Host || Items.empty() || SelectedIndex >= Items.size() || GetIsOpen()) return;

    State = std::make_shared<DropdownState>();
    State->Host = Host;
    State->IsOpen = true;
    State->TriggerRect = GetArrangedRect();
    State->RowHeight = DropdownVisualStyle.RowHeightLogical;
    State->Height = DropdownVisualStyle.RowHeightLogical * static_cast<float>(Items.size()) +
        DropdownVisualStyle.Menu.Padding.Top + DropdownVisualStyle.Menu.Padding.Bottom +
        2.0f * DropdownVisualStyle.Menu.BorderWidth;
    State->WidthTween.Start(DropdownVisualStyle.OpenTransitionSeconds);

    float LabelWidth = 0.0f;
    if (FontBackend) {
        for (const DropdownItem& Item : Items) {
            LabelWidth = std::max(LabelWidth, FontBackend->MeasureTextWidth(Font, Item.Label));
        }
    }
    const BoxStyle& RowStyle = DropdownVisualStyle.Row;
    const float RowFrame = RowStyle.Padding.Left + RowStyle.Padding.Right + 2.0f * RowStyle.BorderWidth;
    State->OpenWidth = std::max(DropdownVisualStyle.MinimumOpenWidthLogical,
                                RowFrame + DropdownVisualStyle.LeadingVisualSizeLogical +
                                    DropdownVisualStyle.LeadingVisualGapLogical + LabelWidth);

    auto Menu = std::make_unique<DropdownMenu>();
    Menu->State = State;
    Menu->Style = DropdownVisualStyle.Menu;
    Menu->Layout = LayoutMode::VerticalStack;
    Menu->CrossAlignment = CrossAlign::Stretch;

    auto AddRow = [&](std::size_t Index, bool Selected) {
        auto Row = std::make_unique<DropdownRow>();
        Row->Item = Items[Index];
        Row->FontBackend = FontBackend;
        Row->Font = Font;
        Row->TextColor = DropdownVisualStyle.ColorText;
        Row->HighlightedTextColor = DropdownVisualStyle.ColorTextHighlighted;
        Row->VisualSize = DropdownVisualStyle.LeadingVisualSizeLogical;
        Row->VisualGap = DropdownVisualStyle.LeadingVisualGapLogical;
        Row->AlwaysHighlighted = Selected;
        Row->Style = Selected ? DropdownVisualStyle.SelectedRow : DropdownVisualStyle.Row;
        Row->Style.Height = Length::Logical(DropdownVisualStyle.RowHeightLogical);
        const std::shared_ptr<DropdownState> SharedState = State;
        Row->OnReleased = [this, SharedState, Index]() {
            if (Index != SelectedIndex) {
                SelectedIndex = Index;
                if (OnSelectionChanged) OnSelectionChanged(Index);
            }
            SharedState->Host->DismissOverlay(SharedState->Id);
        };
        Menu->AddChild(std::move(Row));
    };

    AddRow(SelectedIndex, true);
    for (std::size_t Index = 0; Index < Items.size(); ++Index) {
        if (Index != SelectedIndex) AddRow(Index, false);
    }

    const std::shared_ptr<DropdownState> SharedState = State;
    Menu->OnDestroyed = [SharedState]() { SharedState->IsOpen = false; };
    State->Id = Host->ShowOverlay(std::move(Menu), PanelRect(*State));
}

} // namespace Penumbra::Widgets
