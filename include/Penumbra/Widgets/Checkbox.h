#pragma once

#include "Penumbra/Widgets/Box.h"

#include <functional>

namespace Penumbra::Widgets {

// A boolean toggle with an intrinsic check glyph. The box itself is the glyph: its
// size is GlyphSizeLogical (a value the demo supplies) plus the box-model frame.
class Checkbox : public Box {
public:
    SDL_Color ColorCheckMark{0, 0, 0, 0};
    SDL_Color ColorBoxChecked{0, 0, 0, 0};
    float     GlyphSizeLogical{0.0f};
    bool      Checked{false};

    std::function<void(bool)> OnChanged;

    void ApplyStyle(const CheckboxStyle& Style);

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) override;
    void       DrawContent(Render::Renderer&, SDL_FRect ContentRect) override;

private:
    bool PressedInside{false};
};

} // namespace Penumbra::Widgets
