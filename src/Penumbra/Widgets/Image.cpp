#include "Penumbra/Widgets/Image.h"

#include <utility>

namespace Penumbra::Widgets {

Point Image::MeasureContent(Point /*AvailableContentSize*/) { return SourceSizeLogical; }

void Image::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    if (Texture) {
        Renderer.DrawTexture(Texture, ContentRect);
    }
}

Image::Builder::Builder() : Owned(std::make_unique<Image>()) {}

Image::Builder& Image::Builder::className(std::string Value) {
    Owned->ClassName = std::move(Value);
    return *this;
}

Image::Builder& Image::Builder::child(std::unique_ptr<WidgetBase> Child) {
    Owned->AddChild(std::move(Child));
    return *this;
}

Image::Builder& Image::Builder::children(std::vector<std::unique_ptr<WidgetBase>> Kids) {
    for (auto& Kid : Kids) {
        Owned->AddChild(std::move(Kid));
    }
    return *this;
}

Image::Builder& Image::Builder::onPress(std::function<void()> Handler) {
    Owned->OnPressed = std::move(Handler);
    return *this;
}

Image::Builder& Image::Builder::onRelease(std::function<void()> Handler) {
    Owned->OnReleased = std::move(Handler);
    return *this;
}

Image::Builder& Image::Builder::onHover(std::function<void()> Handler) {
    Owned->OnHovered = std::move(Handler);
    return *this;
}

Image::Builder& Image::Builder::onFocus(std::function<void()> Handler) {
    Owned->OnFocused = std::move(Handler);
    return *this;
}

Image::Builder& Image::Builder::onChange(std::function<void()> Handler) {
    Owned->OnChanged = std::move(Handler);
    return *this;
}

std::unique_ptr<Image> Image::Builder::build() { return std::move(Owned); }

} // namespace Penumbra::Widgets
