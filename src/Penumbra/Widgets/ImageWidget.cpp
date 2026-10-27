#include "Penumbra/Widgets/ImageWidget.h"

#include <utility>

namespace Penumbra::Widgets {

namespace {
constexpr int LeftButton = 0;
} // namespace

void ImageWidget::LoadFrom(Backends::IImageBackend& Backend, SDL_Renderer* Renderer) {
    if (Texture && LoadedWithBackend) {
        LoadedWithBackend->ReleaseTexture(Texture);
    }
    Texture            = nullptr;
    LoadedWithBackend  = nullptr;
    TextureSizeLogical = {0.0f, 0.0f};

    if (FilePath.empty()) {
        return;
    }

    Texture = Backend.LoadTexture(Renderer, FilePath);
    if (!Texture) {
        return;
    }
    LoadedWithBackend = &Backend;

    float TextureWidth = 0.0f, TextureHeight = 0.0f;
    SDL_GetTextureSize(Texture, &TextureWidth, &TextureHeight);
    TextureSizeLogical = {TextureWidth, TextureHeight};
}

Point ImageWidget::Measure(Point /*AvailableSizeLogical*/) {
    return Texture ? TextureSizeLogical : Point{0.0f, 0.0f};
}

void ImageWidget::Arrange(Rect FinalRectLogical) { ArrangedRect = FinalRectLogical; }

bool ImageWidget::UpdateInteractionState(const Platform::InputState& Input) {
    // A leaf with no children to give first refusal to. Same inert-unless-opted-in
    // guard as Box::UpdateInteractionState: skip hit-testing entirely when nothing
    // is listening.
    if (!IsEnabled || !(OnPressed || OnReleased || OnHovered || OnUnhovered || OnFocused || OnChanged)) {
        PressedInside = false;
        if (WasHovered && OnUnhovered) {
            OnUnhovered(); // a widget that goes inert while hovered still ends its hover
        }
        WasHovered = false;
        return false;
    }

    const bool Hovered  = Input.IsPointerOver(ArrangedRect);
    const bool Pressed  = Input.MouseButtonPressedThisFrame[LeftButton];
    const bool Down     = Input.MouseButtonDown[LeftButton];
    const bool Released = Input.MouseButtonReleasedThisFrame[LeftButton];

    if (Hovered && OnHovered) {
        OnHovered();
    }
    if (!Hovered && WasHovered && OnUnhovered) {
        OnUnhovered();
    }
    WasHovered = Hovered;
    if (Pressed && Hovered) {
        PressedInside = true;
        if (OnPressed) {
            OnPressed();
        }
    }
    if (Released) {
        if (PressedInside && OnReleased) {
            OnReleased();
        }
        PressedInside = false;
    }

    return Hovered || (PressedInside && Down);
}

void ImageWidget::Draw(Render::Renderer& Renderer) {
    if (Texture) {
        Renderer.DrawTexture(Texture, ArrangedRect);
    }
}

ImageWidget::~ImageWidget() {
    if (Texture && LoadedWithBackend) {
        LoadedWithBackend->ReleaseTexture(Texture);
    }
}

ImageWidget::Builder::Builder() : Owned(std::make_unique<ImageWidget>()) {}

ImageWidget::Builder& ImageWidget::Builder::src(std::string Value) {
    Owned->FilePath = std::move(Value);
    return *this;
}

ImageWidget::Builder& ImageWidget::Builder::className(std::string Value) {
    Owned->ClassName = std::move(Value);
    return *this;
}

std::unique_ptr<ImageWidget> ImageWidget::Builder::build() { return std::move(Owned); }

} // namespace Penumbra::Widgets
