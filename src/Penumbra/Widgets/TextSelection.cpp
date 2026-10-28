#include "Penumbra/Widgets/TextSelection.h"

#include "Penumbra/Widgets/Checkbox.h"
#include "Penumbra/Widgets/InlineContainer.h"
#include "Penumbra/Widgets/Label.h"
#include "Penumbra/Widgets/TextArea.h"
#include "Penumbra/Widgets/TextInput.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <algorithm>

namespace Penumbra::Widgets {

namespace {

constexpr int LeftButton = 0;

bool Contains(Rect Area, Point At) {
    return At.X >= Area.X && At.X < Area.X + Area.W && At.Y >= Area.Y && At.Y < Area.Y + Area.H;
}

float DistanceOutside(float Value, float Low, float High) {
    if (Value < Low) {
        return Low - Value;
    }
    return Value > High ? Value - High : 0.0f;
}

bool IsInteractive(const WidgetBase& Widget) {
    return Widget.OnPressed || Widget.OnReleased || Widget.OnChanged || dynamic_cast<const TextArea*>(&Widget) ||
           dynamic_cast<const TextInput*>(&Widget) || dynamic_cast<const Checkbox*>(&Widget);
}

bool InteractiveAt(const WidgetBase& Widget, Point At) {
    for (std::size_t Index = 0; Index < Widget.GetChildCount(); ++Index) {
        const WidgetBase* Child = Widget.GetChildAt(Index);
        if (Child == nullptr || !Child->GetIsVisible()) {
            continue;
        }
        if ((Contains(Child->GetArrangedRect(), At) && IsInteractive(*Child)) || InteractiveAt(*Child, At)) {
            return true;
        }
    }
    return false;
}

bool OnSameLine(Rect A, Rect B) {
    const float Overlap = std::min(A.Y + A.H, B.Y + B.H) - std::max(A.Y, B.Y);
    return Overlap > 0.5f * std::min(A.H, B.H);
}

const WidgetBase* ParagraphOf(const WidgetBase& Widget, const WidgetBase* Enclosing) {
    return dynamic_cast<const InlineContainer*>(&Widget) != nullptr ? &Widget : Enclosing;
}

} // namespace

std::vector<TextSelection::Entry> TextSelection::CollectLabels(WidgetBase& Scope) {
    std::vector<Entry> Labels;
    const auto         Visit = [&Labels](const auto& Self, WidgetBase& Widget, const WidgetBase* Paragraph) -> void {
        for (std::size_t Index = 0; Index < Widget.GetChildCount(); ++Index) {
            WidgetBase* Child = Widget.GetChildAt(Index);
            if (Child == nullptr || !Child->GetIsVisible()) {
                continue;
            }
            if (auto* AsLabel = dynamic_cast<Label*>(Child)) {
                Labels.push_back({AsLabel, Paragraph});
            }
            Self(Self, *Child, ParagraphOf(*Child, Paragraph));
        }
    };
    Visit(Visit, Scope, ParagraphOf(Scope, nullptr));
    return Labels;
}

TextSelection::Position TextSelection::PositionAt(const std::vector<Entry>& Labels, Point At) {
    Position Best;
    float    BestRow = 0.0f;
    float    BestColumn = 0.0f;
    bool     Found = false;
    for (std::size_t Index = 0; Index < Labels.size(); ++Index) {
        const Label& Target = *Labels[Index].Target;
        const Rect   Area = Target.GetArrangedRect();
        if (Target.Text.empty() || Area.W <= 0.0f || Area.H <= 0.0f) {
            continue;
        }
        const float Row = DistanceOutside(At.Y, Area.Y, Area.Y + Area.H);
        const float Column = DistanceOutside(At.X, Area.X, Area.X + Area.W);
        if (!Found || Row < BestRow || (Row == BestRow && Column < BestColumn)) {
            Found = true;
            BestRow = Row;
            BestColumn = Column;
            Best.Label = Index;
        }
    }
    if (!Found) {
        return {};
    }
    Label&     Target = *Labels[Best.Label].Target;
    const Rect Area = Target.GetArrangedRect();
    if (At.Y < Area.Y) {
        Best.Index = 0;
    } else if (At.Y >= Area.Y + Area.H) {
        Best.Index = Target.Text.size();
    } else {
        Best.Index = Target.IndexAtPoint(At);
    }
    return Best;
}

bool TextSelection::HasSelection() const { return Anchored && Anchor != Caret; }

void TextSelection::Clear() { Anchored = false; }

void TextSelection::SelectAll(WidgetBase& Scope) {
    const std::vector<Entry> Labels = CollectLabels(Scope);
    Anchored = !Labels.empty();
    if (Anchored) {
        Anchor = {0, 0};
        Caret = {Labels.size() - 1, Labels.back().Target->Text.size()};
    }
    Paint(Labels);
}

std::string TextSelection::SelectedText(WidgetBase& Scope) const { return TextOf(CollectLabels(Scope)); }

std::string TextSelection::TextOf(const std::vector<Entry>& Labels) const {
    if (!HasSelection()) {
        return {};
    }
    const Position    Start = std::min(Anchor, Caret);
    const Position    End = std::max(Anchor, Caret);
    std::string       Result;
    bool              First = true;
    Rect              Previous{};
    const WidgetBase* PreviousParagraph = nullptr;
    for (std::size_t Index = Start.Label; Index <= End.Label && Index < Labels.size(); ++Index) {
        Label&            Target = *Labels[Index].Target;
        const std::size_t From = Index == Start.Label ? std::min(Start.Index, Target.Text.size()) : 0;
        const std::size_t To = Index == End.Label ? std::min(End.Index, Target.Text.size()) : Target.Text.size();
        if (From >= To) {
            continue;
        }
        const Rect Begin = Target.CaretRectAt(From);
        if (!First) {
            if (OnSameLine(Previous, Begin)) {
                if (Begin.X - Previous.X > 0.5f) {
                    Result += ' ';
                }
            } else {
                const bool SameParagraph = Labels[Index].Paragraph != nullptr && Labels[Index].Paragraph == PreviousParagraph;
                Result += SameParagraph ? ' ' : '\n';
            }
        }
        Result.append(Target.Text, From, To - From);
        Previous = Target.CaretRectAt(To);
        PreviousParagraph = Labels[Index].Paragraph;
        First = false;
    }
    return Result;
}

void TextSelection::Paint(const std::vector<Entry>& Labels) {
    const bool     Selected = HasSelection();
    const Position Start = std::min(Anchor, Caret);
    const Position End = std::max(Anchor, Caret);
    Gaps.clear();
    bool First = true;
    Rect Previous{};
    for (std::size_t Index = 0; Index < Labels.size(); ++Index) {
        Label& Target = *Labels[Index].Target;
        if (!Selected || Index < Start.Label || Index > End.Label) {
            Target.ClearSelection();
            continue;
        }
        const std::size_t From = Index == Start.Label ? std::min(Start.Index, Target.Text.size()) : 0;
        const std::size_t To = Index == End.Label ? std::min(End.Index, Target.Text.size()) : Target.Text.size();
        Target.SetSelection(From, To, ColorSelection);
        if (From >= To) {
            continue;
        }
        const Rect Begin = Target.CaretRectAt(From);
        if (!First && OnSameLine(Previous, Begin) && Begin.X > Previous.X) {
            const float Top = std::min(Previous.Y, Begin.Y);
            const float Bottom = std::max(Previous.Y + Previous.H, Begin.Y + Begin.H);
            Gaps.push_back({Previous.X, Top, Begin.X - Previous.X, Bottom - Top});
        }
        Previous = Target.CaretRectAt(To);
        First = false;
    }
    Painted = Selected;
}

void TextSelection::DrawGaps(Render::Renderer& Renderer) const {
    for (const Rect& Gap : Gaps) {
        Renderer.DrawFilledRect(Gap, ColorSelection);
    }
}

void TextSelection::Update(WidgetBase& Scope, Rect Region, const Platform::InputState& Input) {
    const bool         Pressed = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool         Down = Input.MouseButtonDown[LeftButton];
    std::vector<Entry> Labels;
    bool               Collected = false;
    const auto         Ensure = [&]() -> const std::vector<Entry>& {
        if (!Collected) {
            Labels = CollectLabels(Scope);
            Collected = true;
        }
        return Labels;
    };

    if (Pressed) {
        Anchored = false;
        Dragging = false;
        if (Input.IsPointerOver(Region) && !InteractiveAt(Scope, Input.MousePosition)) {
            Anchor = PositionAt(Ensure(), Input.MousePosition);
            Caret = Anchor;
            Anchored = true;
            Dragging = true;
            if (Focus != nullptr) {
                Focus->Focused = &Scope;
            }
        }
    } else if (Dragging && Down) {
        Caret = PositionAt(Ensure(), Input.MousePosition);
    }
    if (!Down) {
        Dragging = false;
    }

    if (Focus == nullptr || Focus->Focused == &Scope) {
        for (const Platform::KeyEvent& Press : Input.KeyPressesInOrder()) {
            const Platform::Key Key      = Press.Code;
            const bool          Shortcut = Press.ModifierState.Shortcut;
            if (Key == Platform::Key::Escape) {
                Anchored = false;
            } else if (Shortcut && Key == Platform::Key::A) {
                SelectAll(Scope);
                Collected = false;
            } else if (Shortcut && Key == Platform::Key::C && HasSelection() && Clipboard) {
                Clipboard->SetClipboardText(TextOf(Ensure()));
            }
        }
    }

    if (HasSelection() || Painted) {
        Paint(Ensure());
    }
}

} // namespace Penumbra::Widgets
