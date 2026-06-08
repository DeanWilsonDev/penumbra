#pragma once

#include "Penumbra/Render/IFontBackend.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace Penumbra::Render {

// SDL_ttf implementation of IFontBackend. Owns its TTF_Font objects and a simple
// texture cache keyed by (font, text, color); both are torn down in the destructor,
// which must run while the SDL_Renderer that created the textures is still alive.
class SdlTtfFontBackend : public IFontBackend {
public:
    SdlTtfFontBackend();
    ~SdlTtfFontBackend() override;

    SdlTtfFontBackend(const SdlTtfFontBackend&) = delete;
    SdlTtfFontBackend& operator=(const SdlTtfFontBackend&) = delete;

    FontHandle   LoadFont(const char* Path, float PointSizeLogical, float DpiScaleFactor) override;
    TextMetrics  MeasureText     (FontHandle, std::string_view) const override;
    float        MeasureTextWidth(FontHandle, std::string_view) const override;
    SDL_Texture* AcquireTextTexture(SDL_Renderer*, FontHandle, std::string_view, SDL_Color) override;

private:
    struct LoadedFont {
        TTF_Font* Font;
        float     DpiScaleFactor;
    };

    const LoadedFont* GetFont(FontHandle Handle) const;

    std::vector<LoadedFont>                       Fonts;
    std::unordered_map<std::string, SDL_Texture*> TextureCache;
    bool                                          TtfInitialised{false};
};

} // namespace Penumbra::Render
