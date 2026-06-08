#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string_view>

namespace Penumbra::Render {

using FontHandle = uint32_t; // opaque handle into the font backend

struct TextMetrics {
    float WidthLogical;
    float HeightLogical;
    float AscentLogical;
};

// Abstraction over the text rasteriser. SDL_ttf today; a FreeType + glyph-atlas
// upgrade later should touch only the implementation, not this interface, and not
// any widget. The backend rasterises at physical size and reports in LOGICAL units.
class IFontBackend {
public:
    virtual ~IFontBackend() = default;

    // Loads a font at a logical point size. The backend rasterises at
    // PointSizeLogical * DpiScaleFactor so glyph textures are physical-pixel sharp.
    virtual FontHandle LoadFont(const char* Path, float PointSizeLogical, float DpiScaleFactor) = 0;

    virtual TextMetrics MeasureText     (FontHandle, std::string_view) const = 0;
    virtual float       MeasureTextWidth(FontHandle, std::string_view) const = 0;

    // Produces a texture for a run of text. Ownership stays with the backend;
    // callers must not destroy the returned texture. Caching is an implementation
    // detail of the backend.
    virtual SDL_Texture* AcquireTextTexture(SDL_Renderer*, FontHandle, std::string_view, SDL_Color) = 0;
};

} // namespace Penumbra::Render
