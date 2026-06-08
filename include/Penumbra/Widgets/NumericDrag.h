#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"

#include <functional>

namespace Penumbra::Widgets {

// Blender-style click-and-drag to change a float. Displays its value as intrinsic
// text; horizontal drag adjusts it by Sensitivity (value per logical pixel).
class NumericDrag : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    SDL_Color             ColorText{0, 0, 0, 0};

    float Value{0.0f};
    float Sensitivity{0.0f};            // value change per logical px dragged
    float PreferredWidthLogical{0.0f};  // demo-supplied minimum field width

    std::function<void(float)> OnValueChanged;

    bool UpdateInteractionState(const Platform::InputState&) override;

protected:
    SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) override;
    void       DrawContent(Render::Renderer&, SDL_FRect ContentRect) override;

private:
    std::string FormatValue() const;

    bool  Dragging{false};
    float LastMouseX{0.0f};
};

} // namespace Penumbra::Widgets
