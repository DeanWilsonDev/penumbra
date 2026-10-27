#pragma once

#include "Penumbra/Platform/IClipboard.h"
#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"
#include "Penumbra/Widgets/FocusState.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>

namespace Penumbra::Widgets {

class TextInput : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    Render::Color         ColorText{0, 0, 0, 0};
    Render::Color         ColorCaret{0, 0, 0, 0};
    Render::Color         ColorSelection{0, 0, 0, 0};
    float                 CaretWidthLogical{0.0f};   // demo-supplied
    float                 PreferredWidthLogical{0.0f}; // demo-supplied field width

    FocusState*           Focus{nullptr};
    Platform::IClipboard* Clipboard{nullptr};
    std::string           Text;

    std::function<void(const std::string&)> OnTextChanged;
    std::function<void()>                   OnFocusLost; // the frame after focus moved elsewhere

    ~TextInput() override; // gives up focus, so a later widget at this address isn't focused

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;

private:
    bool IsFocused() const { return Focus != nullptr && Focus->Focused == this; }
    bool WasFocused{false};

    bool        HasSelection() const { return SelectionAnchor != CaretIndex; }
    std::size_t SelectionStart() const { return std::min(SelectionAnchor, CaretIndex); }
    std::size_t SelectionEnd() const { return std::max(SelectionAnchor, CaretIndex); }
    void        DeleteSelection();
    std::size_t CaretIndexAtX(float LocalX) const;
    float       TextWidthTo(std::size_t Index) const;

    std::size_t CaretIndex{0};
    std::size_t SelectionAnchor{0}; // == CaretIndex means no selection
    bool        Dragging{false};
};

} // namespace Penumbra::Widgets
