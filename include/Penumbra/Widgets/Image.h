#pragma once

#include "Penumbra/Widgets/Box.h"

#include <SDL3/SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Penumbra::Widgets {

// A single decoded image, drawn stretched to fill its content rect. A leaf,
// like Label. Penumbra doesn't decode image files — no format/codec opinion,
// consistent with "no defaults, no opinions" — so Texture is an SDL_Texture
// the caller already created (e.g. via SDL_image) and keeps alive; Image only
// draws it, the same non-owning-pointer contract Label::FontBackend already
// uses for an externally-owned resource. SourceSizeLogical is supplied
// alongside it for the same reason Label needs a font backend at Measure
// time: Measure has no Renderer to query a texture's size from, so the caller
// reports it directly (the same "demo/consumer supplies the value" idiom as
// Checkbox::GlyphSizeLogical or TextInput::PreferredWidthLogical).
class Image : public Box {
public:
    SDL_Texture* Texture{nullptr};
    Point        SourceSizeLogical{0.0f, 0.0f};

    // Fluent, chainable construction — see Box::Builder for the naming-
    // convention rationale (method names match Iris prop names exactly,
    // className() aside). Texture/SourceSizeLogical have no builder method:
    // Iris's <Image> prop name for image content isn't specified yet, and
    // guessing one would risk diverging from the eventual codegen mapping —
    // set them directly on the built widget until that's pinned down.
    class Builder {
    public:
        Builder();

        Builder& className(std::string Value);
        Builder& child(std::unique_ptr<WidgetBase> Child);
        Builder& children(std::vector<std::unique_ptr<WidgetBase>> Kids);
        Builder& onPress(std::function<void()> Handler);
        Builder& onRelease(std::function<void()> Handler);
        Builder& onHover(std::function<void()> Handler);
        Builder& onFocus(std::function<void()> Handler);
        Builder& onChange(std::function<void()> Handler);

        std::unique_ptr<Image> build();

    private:
        std::unique_ptr<Image> Owned;
    };

protected:
    Point MeasureContent(Point AvailableContentSize) override;
    void  DrawContent(Render::Renderer&, Rect ContentRect) override;
};

} // namespace Penumbra::Widgets
