#pragma once

#include "Penumbra/Backends/IImageBackend.h"
#include "Penumbra/Widgets/WidgetBase.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace Penumbra::Widgets {

// A leaf widget that loads and draws a single image file. A WidgetBase subclass
// directly rather than a Box — this minimal cut has no box model of its own (no
// background/border/padding). Loading goes through IImageBackend (SdlImageBackend
// today), mirroring how Label goes through IFontBackend instead of calling
// SDL_ttf directly, so the concrete image loader can be swapped without touching
// widget code.
class ImageWidget : public WidgetBase {
public:
    std::string  FilePath;
    SDL_Texture* Texture{nullptr};

    // Loads FilePath through Backend, replacing (and releasing) whatever texture
    // this widget currently holds. A no-op if FilePath is empty. Measure/Arrange/
    // Draw never load on their own — call this first (e.g. once up front, or
    // again after changing FilePath).
    void LoadFrom(Backends::IImageBackend& Backend, SDL_Renderer* Renderer);

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

    ~ImageWidget() override;

    // Fluent, chainable construction. Deliberately narrow: just src() and
    // className(); no
    // child()/children() (a leaf; GetChildCount/GetChildAt inherit WidgetBase's
    // zero/nullptr default) and no onPress()/onRelease()/etc, per the ImageWidget
    // requirements doc's own builder method table.
    class Builder {
    public:
        Builder();

        Builder& src(std::string Value);
        Builder& className(std::string Value);

        std::unique_ptr<ImageWidget> build();

    private:
        std::unique_ptr<ImageWidget> Owned;
    };

private:
    // The backend Texture was loaded through, kept only so LoadFrom (on reload)
    // and the destructor can release it correctly — the same "widget owns the
    // SDL resource it acquired" idiom ViewportWidget already uses for its own
    // scene texture, not a caching/asset-management layer.
    Backends::IImageBackend* LoadedWithBackend{nullptr};
    Point                    TextureSizeLogical{0.0f, 0.0f};
    bool                     PressedInside{false};
    bool                     WasHovered{false};
};

} // namespace Penumbra::Widgets
