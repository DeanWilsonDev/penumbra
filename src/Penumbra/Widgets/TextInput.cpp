#include "Penumbra/Widgets/TextInput.h"

#include "Penumbra/Widgets/KeyDispatch.h"

#include "Penumbra/Render/Utf8.h"

#include <cmath>
#include <string_view>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
} // namespace

float TextInput::TextWidthTo(std::size_t Index) const {
    if (!FontBackend || Index == 0) {
        return 0.0f;
    }
    return FontBackend->MeasureTextWidth(Font, std::string_view(Text).substr(0, Index));
}

std::size_t TextInput::CaretIndexAtX(float LocalX) const {
    if (!FontBackend || Text.empty() || LocalX <= 0.0f) {
        return 0;
    }
    std::size_t Best = 0;
    float BestDistance = std::fabs(LocalX);
    for (std::size_t Index = 0; Index < Text.size();) {
        Index = Render::NextCodePoint(Text, Index);
        const float Distance = std::fabs(LocalX - TextWidthTo(Index));
        if (Distance < BestDistance) {
            BestDistance = Distance;
            Best = Index;
        }
    }
    return Best;
}

void TextInput::DeleteSelection() {
    const std::size_t Start = SelectionStart();
    Text.erase(Start, SelectionEnd() - Start);
    CaretIndex = Start;
    SelectionAnchor = Start;
}

Point TextInput::MeasureContent(Point /*AvailableContentSize*/) {
    float LineHeight = 0.0f;
    if (FontBackend) {
        LineHeight = FontBackend->MeasureText(Font, "Ag").HeightLogical;
    }
    return {PreferredWidthLogical, LineHeight};
}

TextInput::~TextInput() {
    if (IsFocused()) {
        Focus->Focused = nullptr;
    }
}

bool TextInput::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Hovered  = Input.IsPointerOver(ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];
    const Rect Content = ContentRectFrom(ArrangedRect);

    if (Pressed && Hovered && Focus) {
        Focus->Focused = this;
        const std::size_t Index = CaretIndexAtX(Input.MousePosition.X - Content.X);
        CaretIndex = Index;
        SelectionAnchor = Index; // fresh selection at the click point
        Dragging = true;
    }
    if (Dragging && Down && IsFocused()) {
        CaretIndex = CaretIndexAtX(Input.MousePosition.X - Content.X); // drag extends selection
    }
    if (Released) {
        Dragging = false;
    }
    const bool FocusedNow = IsFocused();
    if (WasFocused && !FocusedNow && OnFocusLost) {
        OnFocusLost();
    }
    WasFocused = FocusedNow;

    if (!IsFocused()) {
        return Hovered;
    }

    const bool Shortcut = Input.ModifierState.Shortcut;
    const bool Shift    = Input.ModifierState.Shift;
    bool Changed = false;

    // Typed text replaces any selection.
    if (!Input.TextInputThisFrame.empty()) {
        if (HasSelection()) {
            DeleteSelection();
        }
        Text.insert(CaretIndex, Input.TextInputThisFrame);
        CaretIndex += Input.TextInputThisFrame.size();
        SelectionAnchor = CaretIndex;
        Changed = true;
    }

    for (const Platform::Key Key : Input.KeysPressedThisFrame) {
        // A single-line field types and edits, but has no use for Escape, Tab, Enter, Up
        // or Down, nor any shortcut but select-all, copy, cut and paste: those go on to
        // whatever handles keys (KeyDispatch).
        const bool Uses = Shortcut ? (Key == Platform::Key::A || Key == Platform::Key::C ||
                                      Key == Platform::Key::X || Key == Platform::Key::V)
                                   : (Key != Platform::Key::Escape && Key != Platform::Key::Tab &&
                                      Key != Platform::Key::Enter && Key != Platform::Key::Up &&
                                      Key != Platform::Key::Down);
        if (Uses) {
            KeyDispatch::MarkUsed(Key);
        }
        if (Shortcut) {
            switch (Key) {
            case Platform::Key::A:
                SelectionAnchor = 0;
                CaretIndex = Text.size();
                break;
            case Platform::Key::C:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                }
                break;
            case Platform::Key::X:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                    DeleteSelection();
                    Changed = true;
                }
                break;
            case Platform::Key::V:
                if (Clipboard) {
                    const std::string Pasted = Clipboard->GetClipboardText();
                    if (!Pasted.empty()) {
                        if (HasSelection()) {
                            DeleteSelection();
                        }
                        Text.insert(CaretIndex, Pasted);
                        CaretIndex += Pasted.size();
                        SelectionAnchor = CaretIndex;
                        Changed = true;
                    }
                }
                break;
            default:
                break;
            }
            continue;
        }

        switch (Key) {
        case Platform::Key::Backspace:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex > 0) {
                const std::size_t Previous = Render::PreviousCodePoint(Text, CaretIndex);
                Text.erase(Previous, CaretIndex - Previous);
                CaretIndex = Previous;
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            break;
        case Platform::Key::Delete:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex < Text.size()) {
                Text.erase(CaretIndex, Render::NextCodePoint(Text, CaretIndex) - CaretIndex);
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            break;
        case Platform::Key::Left:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionStart();
            } else {
                CaretIndex = Render::PreviousCodePoint(Text, CaretIndex);
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case Platform::Key::Right:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionEnd();
            } else {
                CaretIndex = Render::NextCodePoint(Text, CaretIndex);
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case Platform::Key::Home:
            CaretIndex = 0;
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case Platform::Key::End:
            CaretIndex = Text.size();
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        default:
            break;
        }
    }

    if (Changed && OnTextChanged) {
        OnTextChanged(Text);
    }
    return true; // a focused field consumes input
}

void TextInput::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    // Clip so text/selection longer than the field do not spill past the content rect.
    Renderer.PushClipRect(ContentRect);

    const float LineHeight =
        FontBackend ? FontBackend->MeasureText(Font, "Ag").HeightLogical : ContentRect.H;

    if (IsFocused() && HasSelection()) {
        const float X0 = ContentRect.X + TextWidthTo(SelectionStart());
        const float X1 = ContentRect.X + TextWidthTo(SelectionEnd());
        Renderer.DrawFilledRect({X0, ContentRect.Y, X1 - X0, LineHeight}, ColorSelection);
    }

    Renderer.DrawText(Font, Text, {ContentRect.X, ContentRect.Y}, ColorText);

    if (IsFocused()) {
        const float CaretX = ContentRect.X + TextWidthTo(CaretIndex);
        Renderer.DrawFilledRect({CaretX, ContentRect.Y, CaretWidthLogical, LineHeight}, ColorCaret);
    }

    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
