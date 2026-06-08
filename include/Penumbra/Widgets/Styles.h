#pragma once

#include <SDL3/SDL.h>

namespace Penumbra::Widgets {

struct EdgeInsets {
    float Left;
    float Top;
    float Right;
    float Bottom;
};

enum class LayoutMode { None, VerticalStack, HorizontalStack };
enum class CrossAlign { Start, Center, End, Stretch };

// The universal style slots — the "tokens" every widget honours. Penumbra defines
// the SHAPE; it supplies NO values and NO semantic names. Default-constructed it is
// all-zero (transparent, no border, no spacing): the absence of styling, not an
// opinion. The demo fills in every value it cares about.
struct BoxStyle {
    SDL_Color  ColorBackground{0, 0, 0, 0};
    SDL_Color  ColorBorder{0, 0, 0, 0};
    float      BorderWidth{0.0f};
    float      BorderRadius{0.0f}; // honoured by API; rendered square in first cut
    EdgeInsets Padding{0.0f, 0.0f, 0.0f, 0.0f}; // inside the border  — the box's own job
    EdgeInsets Margin{0.0f, 0.0f, 0.0f, 0.0f};  // outside the border — the PARENT's job
};

// Per-widget styles extend BoxStyle so the box-model slots stay universal and free.
struct ButtonStyle : BoxStyle {
    SDL_Color ColorBackgroundHovered{0, 0, 0, 0};
    SDL_Color ColorBackgroundPressed{0, 0, 0, 0};
    SDL_Color ColorBackgroundDisabled{0, 0, 0, 0};
    SDL_Color ColorLabel{0, 0, 0, 0}; // applied to a Label child by the resolver, not by Button
};

struct CheckboxStyle : BoxStyle {
    SDL_Color ColorCheckMark{0, 0, 0, 0};
    SDL_Color ColorBoxChecked{0, 0, 0, 0};
};

} // namespace Penumbra::Widgets
