#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/FocusState.h"

#include <cstddef>
#include <functional>
#include <string>

namespace Penumbra::Widgets {

// Single-line text entry (PoC scope): visible caret; type / backspace / delete;
// left / right arrow movement. Deferred: selection, clipboard, IME, multi-line.
// Requests focus on click; only acts on text/key events while it holds focus.
class TextInput : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    SDL_Color             ColorText{0, 0, 0, 0};
    SDL_Color             ColorCaret{0, 0, 0, 0};
    float                 CaretWidthLogical{0.0f};   // demo-supplied
    float                 PreferredWidthLogical{0.0f}; // demo-supplied field width

    FocusState* Focus{nullptr};
    std::string Text;

    std::function<void(const std::string&)> OnTextChanged;

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) override;
    void       DrawContent(Render::Renderer&, SDL_FRect ContentRect) override;

private:
    bool IsFocused() const { return Focus != nullptr && Focus->Focused == this; }

    std::size_t CaretIndex{0};
};

} // namespace Penumbra::Widgets
