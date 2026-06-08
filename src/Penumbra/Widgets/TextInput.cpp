#include "Penumbra/Widgets/TextInput.h"

#include <algorithm>

namespace Penumbra::Widgets {

namespace {
bool PointInRect(SDL_FPoint Point, SDL_FRect Rect) {
    return Point.x >= Rect.x && Point.x < Rect.x + Rect.w &&
           Point.y >= Rect.y && Point.y < Rect.y + Rect.h;
}
constexpr int LeftButton = 0;
} // namespace

SDL_FPoint TextInput::MeasureContent(SDL_FPoint /*AvailableContentSize*/) {
    float LineHeight = 0.0f;
    if (FontBackend) {
        LineHeight = FontBackend->MeasureText(Font, "Ag").HeightLogical;
    }
    return {PreferredWidthLogical, LineHeight};
}

bool TextInput::UpdateInteractionState(const Platform::InputState& Input) {
    const bool Hovered = PointInRect(Input.MousePosition, ArrangedRect);

    if (Input.MouseButtonPressedThisFrame[LeftButton] && Hovered && Focus) {
        Focus->Focused = this; // claim focus
        CaretIndex = Text.size();
    }

    if (!IsFocused()) {
        return Hovered;
    }

    bool Changed = false;

    if (!Input.TextInputThisFrame.empty()) {
        Text.insert(CaretIndex, Input.TextInputThisFrame);
        CaretIndex += Input.TextInputThisFrame.size();
        Changed = true;
    }

    for (const SDL_Keycode Key : Input.KeysPressedThisFrame) {
        switch (Key) {
        case SDLK_BACKSPACE:
            if (CaretIndex > 0) {
                Text.erase(CaretIndex - 1, 1);
                --CaretIndex;
                Changed = true;
            }
            break;
        case SDLK_DELETE:
            if (CaretIndex < Text.size()) {
                Text.erase(CaretIndex, 1);
                Changed = true;
            }
            break;
        case SDLK_LEFT:
            if (CaretIndex > 0) {
                --CaretIndex;
            }
            break;
        case SDLK_RIGHT:
            if (CaretIndex < Text.size()) {
                ++CaretIndex;
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
    // Clip so text longer than the field does not spill past its content rect.
    Renderer.PushClipRect(ContentRect);

    Renderer.DrawText(Font, Text, {ContentRect.x, ContentRect.y}, ColorText);

    if (IsFocused() && FontBackend) {
        const float CaretX =
            ContentRect.x + FontBackend->MeasureTextWidth(Font, Text.substr(0, CaretIndex));
        const float LineHeight = FontBackend->MeasureText(Font, "Ag").HeightLogical;
        Renderer.DrawFilledRect({CaretX, ContentRect.y, CaretWidthLogical, LineHeight}, ColorCaret);
    }

    Renderer.PopClipRect();
}

} // namespace Penumbra::Widgets
