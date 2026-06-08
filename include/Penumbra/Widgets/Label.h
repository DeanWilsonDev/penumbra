#pragma once

#include "Penumbra/Render/IFontBackend.h"
#include "Penumbra/Widgets/Box.h"

#include <string>

namespace Penumbra::Widgets {

// Intrinsic text content; a leaf. The font backend is injected (the Measure pass
// has no Renderer, yet text size must be known there) and identifies the font the
// Renderer was initialised with.
class Label : public Box {
public:
    Render::IFontBackend* FontBackend{nullptr};
    Render::FontHandle    Font{0};
    std::string           Text;
    SDL_Color             ColorText{0, 0, 0, 0};

protected:
    SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) override;
    void       DrawContent(Render::Renderer&, SDL_FRect ContentRect) override;
};

} // namespace Penumbra::Widgets
