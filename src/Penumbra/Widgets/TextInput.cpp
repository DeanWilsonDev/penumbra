#include "Penumbra/Widgets/TextInput.h"

#include <cmath>
#include <string_view>

namespace Penumbra::Widgets {

namespace {
bool PointInRect(SDL_FPoint Point, SDL_FRect Rect) {
    return Point.x >= Rect.x && Point.x < Rect.x + Rect.w &&
           Point.y >= Rect.y && Point.y < Rect.y + Rect.h;
}
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
    // Pick the character boundary whose x is nearest the click.
    std::size_t Best = 0;
    float BestDistance = std::fabs(LocalX);
    for (std::size_t Index = 1; Index <= Text.size(); ++Index) {
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

SDL_FPoint TextInput::MeasureContent(SDL_FPoint /*AvailableContentSize*/) {
    float LineHeight = 0.0f;
    if (FontBackend) {
        LineHeight = FontBackend->MeasureText(Font, "Ag").HeightLogical;
    }
    return {PreferredWidthLogical, LineHeight};
}

bool TextInput::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Hovered  = PointInRect(Input.MousePosition, ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];
    const SDL_FRect Content = ContentRectFrom(ArrangedRect);

    if (Pressed && Hovered && Focus) {
        Focus->Focused = this;
        const std::size_t Index = CaretIndexAtX(Input.MousePosition.x - Content.x);
        CaretIndex = Index;
        SelectionAnchor = Index; // fresh selection at the click point
        Dragging = true;
    }
    if (Dragging && Down && IsFocused()) {
        CaretIndex = CaretIndexAtX(Input.MousePosition.x - Content.x); // drag extends selection
    }
    if (Released) {
        Dragging = false;
    }

    if (!IsFocused()) {
        return Hovered;
    }

    const bool Ctrl  = (Input.ModifierState & SDL_KMOD_CTRL) != 0;
    const bool Shift = (Input.ModifierState & SDL_KMOD_SHIFT) != 0;
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

    for (const SDL_Keycode Key : Input.KeysPressedThisFrame) {
        if (Ctrl) {
            switch (Key) {
            case SDLK_A:
                SelectionAnchor = 0;
                CaretIndex = Text.size();
                break;
            case SDLK_C:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                }
                break;
            case SDLK_X:
                if (HasSelection() && Clipboard) {
                    Clipboard->SetClipboardText(Text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                    DeleteSelection();
                    Changed = true;
                }
                break;
            case SDLK_V:
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
            continue; // Ctrl combos never fall through to plain editing/navigation
        }

        switch (Key) {
        case SDLK_BACKSPACE:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex > 0) {
                Text.erase(CaretIndex - 1, 1);
                --CaretIndex;
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            break;
        case SDLK_DELETE:
            if (HasSelection()) {
                DeleteSelection();
                Changed = true;
            } else if (CaretIndex < Text.size()) {
                Text.erase(CaretIndex, 1);
                Changed = true;
            }
            SelectionAnchor = CaretIndex;
            break;
        case SDLK_LEFT:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionStart();
            } else if (CaretIndex > 0) {
                --CaretIndex;
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case SDLK_RIGHT:
            if (!Shift && HasSelection()) {
                CaretIndex = SelectionEnd();
            } else if (CaretIndex < Text.size()) {
                ++CaretIndex;
            }
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case SDLK_HOME:
            CaretIndex = 0;
            if (!Shift) {
                SelectionAnchor = CaretIndex;
            }
            break;
        case SDLK_END:
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

void TextInput::DrawContent(Render::Renderer& Renderer, SDL_FRect ContentRect) {
    // Clip so text/selection longer than the field do not spill past the content rect.
    Renderer.PushClipRect(ContentRect);

    const float LineHeight =
        FontBackend ? FontBackend->MeasureText(Font, "Ag").HeightLogical : ContentRect.h;

    if (IsFocused() && HasSelection()) {
        const float X0 = ContentRect.x + TextWidthTo(SelectionStart());
        const float X1 = ContentRect.x + TextWidthTo(SelectionEnd());
        Renderer.DrawFilledRect({X0, ContentRect.y, X1 - X0, LineHeight}, ColorSelection);
    }

    Renderer.DrawText(Font, Text, {ContentRect.x, ContentRect.y}, ColorText);

    if (IsFocused()) {
        const float CaretX = ContentRect.x + TextWidthTo(CaretIndex);
        Renderer.DrawFilledRect({CaretX, ContentRect.y, CaretWidthLogical, LineHeight}, ColorCaret);
    }

    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
