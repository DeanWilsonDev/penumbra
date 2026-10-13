#pragma once

#include <cstdint>

namespace Penumbra::Render {

// Penumbra's own color type. Same layout as SDL_Color, but owned by Penumbra
// so nothing above the Platform layer needs an SDL header to spell a color.
struct Color {
    std::uint8_t R{0};
    std::uint8_t G{0};
    std::uint8_t B{0};
    std::uint8_t A{0};

    friend bool operator==(const Color&, const Color&) = default;
};

} // namespace Penumbra::Render
