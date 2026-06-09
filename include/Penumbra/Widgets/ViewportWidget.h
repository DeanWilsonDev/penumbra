#pragma once

#include "Penumbra/Widgets/Box.h"

#include <functional>

namespace Penumbra::Widgets {

// The rendering seam between Penumbra's UI pass and a consumer's scene pass (Dawn).
// A ViewportWidget is just a Box in the tree — position, size, BoxStyle, normal
// two-phase layout. What makes it special is its DrawContent: it redirects the SDL
// render target to an off-screen texture, lets the consumer draw the scene through
// the ordinary Render::Renderer interface, then restores the target and blits the
// texture into the content rect. The consumer never calls SDL; all render-target
// management is locked inside this widget.
class ViewportWidget : public Box {
public:
    // Called every frame to let the consumer draw scene content.
    // Renderer is already targeting the scene texture when this fires.
    // SceneSizeLogical is the content rect size in logical pixels — use it
    // to derive viewport bounds, not any SDL query.
    std::function<void(Render::Renderer&, SDL_FPoint SceneSizeLogical)> OnRenderScene;

    // Called when a mouse event falls inside the content rect and was not consumed
    // by any child widget (UI overlays inside the viewport get first refusal).
    // ContentRect is the logical content area; the consumer uses it to map screen
    // coordinates to scene coordinates. Returns true if the event was consumed.
    std::function<bool(const Platform::InputState&, SDL_FRect ContentRect)> OnSceneInput;

    // Color the scene texture is cleared to before OnRenderScene fires.
    // No default opinion beyond opaque black — the consumer sets this.
    SDL_Color SceneClearColor{0, 0, 0, 255};

    SDL_FPoint Measure(SDL_FPoint AvailableSizeLogical) override;
    void       Arrange(SDL_FRect FinalRectLogical) override;
    bool       UpdateInteractionState(const Platform::InputState&) override;

    ~ViewportWidget() override;

protected:
    // The scene render-to-texture + blit happens here, the standard Box extension
    // point. Box::Draw frames the widget (background, border), calls this to composite
    // the scene into the content rect, then draws children as overlays on top.
    void DrawContent(Render::Renderer&, SDL_FRect ContentRect) override;

private:
    void RecreateSceneTexture(Render::Renderer&);

    SDL_Texture* SceneTexture{nullptr};
    SDL_FPoint   SceneTextureSizePhysical{0.0f, 0.0f}; // tracks current texture size
    SDL_FPoint   ContentSizeLogical{0.0f, 0.0f};       // from last Arrange
};

} // namespace Penumbra::Widgets
