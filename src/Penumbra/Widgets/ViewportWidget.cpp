#include "Penumbra/Widgets/ViewportWidget.h"

#include "Penumbra/Widgets/Stacking.h"

#include <cmath>

namespace Penumbra::Widgets {

namespace {

// Logical size × dpi, rounded to whole physical pixels — how big the scene texture
// must be to stay crisp at the current display density (spec §4, §6).
Point PhysicalSizeFor(Point ContentSizeLogical, float DpiScale) {
    return {std::round(ContentSizeLogical.X * DpiScale),
            std::round(ContentSizeLogical.Y * DpiScale)};
}

} // namespace

ViewportWidget::~ViewportWidget() {
    if (SceneTexture) {
        SDL_DestroyTexture(SceneTexture);
        SceneTexture = nullptr;
    }
}

Point ViewportWidget::Measure(Point AvailableSizeLogical) {
    // A viewport is greedy: the scene has no intrinsic size, so the widget takes
    // whatever area its parent offers. The consumer's layout decides the bounds.
    return AvailableSizeLogical;
}

void ViewportWidget::Arrange(Rect FinalRectLogical) {
    // Box::Arrange commits ArrangedRect and lays out any overlay children per Layout.
    Box::Arrange(FinalRectLogical);
    const Rect Content = ContentRectFrom(FinalRectLogical);
    ContentSizeLogical = {Content.W, Content.H};
}

bool ViewportWidget::UpdateInteractionState(const Platform::InputState& Given) {
    if (StackingContext::IsLayeredOut(*this)) {
        return false; // its stacking context hands it input itself, in paint order
    }
    Platform::InputState        PointerEventsInput;
    const Platform::InputState& Input = InputForPointerEvents(Style.PointerEventsMode, Given, PointerEventsInput);

    // Layered descendants first, then overlay children (toolbars, gizmo panels)
    // front-to-back. They still update when the pointer is elsewhere, or once one has
    // taken it, with the pointer occluded.
    StackingContext         Context(*this, StackingContext::Pass::Input);
    const bool              Over = Input.IsPointerOver(ArrangedRect);
    Platform::OccludedInput Occluded(Input);
    const bool              LayeredTook   = Context.DispatchAbove(Input);
    bool                    ChildConsumed = LayeredTook;
    for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator) {
        if (!(*Iterator)->GetIsVisible() || StackingContext::IsLayeredOut(**Iterator)) {
            continue; // hidden, or its stacking context hands it input itself
        }
        const bool Occlude = !Over || ChildConsumed;
        Platform::InputState ChildInput;
        if ((*Iterator)->UpdateInteractionState(InputForChild(**Iterator, Occlude ? Occluded.Get() : Input, ChildInput)) &&
            !Occlude) {
            ChildConsumed = true;
        }
    }
    if (Context.DispatchBelow(Input, ChildConsumed)) {
        ChildConsumed = true;
    }

    // A layered descendant can take the pointer outside the viewport's own rect.
    if (LayeredTook) {
        return true;
    }
    // Outside the widget entirely → no consumption, input falls through (spec §7.1).
    if (!Over) {
        return false;
    }
    if (ChildConsumed) {
        return true;
    }

    // Inside the scene area and unclaimed by an overlay → the consumer handles it.
    const Rect Content = ContentRectFrom(ArrangedRect);
    if (Input.IsPointerOver(Content) && OnSceneInput) {
        return OnSceneInput(Input, Content);
    }

    // Being over the viewport is itself opaque — prevents fall-through to whatever is
    // behind it in the tree, even over the border/padding ring (spec §7.4).
    return true;
}

void ViewportWidget::RecreateSceneTexture(Render::Renderer& Renderer) {
    if (SceneTexture) {
        SDL_DestroyTexture(SceneTexture); // old texture goes before the new one (spec §4)
        SceneTexture = nullptr;
    }

    const Point Physical = PhysicalSizeFor(ContentSizeLogical, Renderer.GetDpiScaleFactor());
    SceneTextureSizePhysical = Physical;

    const int Width  = static_cast<int>(Physical.X);
    const int Height = static_cast<int>(Physical.Y);
    if (Width <= 0 || Height <= 0) {
        return; // collapsed or not yet arranged — no texture this frame
    }

    SceneTexture = SDL_CreateTexture(Renderer.GetSdlRenderer(), SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_TARGET, Width, Height);
    if (SceneTexture) {
        // Honour the scene's alpha when blitting back, so a transparent clear colour
        // lets the UI background show through and an opaque one covers it cleanly.
        SDL_SetTextureBlendMode(SceneTexture, SDL_BLENDMODE_BLEND);
    }
}

void ViewportWidget::DrawContent(Render::Renderer& Renderer, Rect ContentRect) {
    // 1. Recreate the texture if the content size changed since the last frame.
    const Point Physical =
        PhysicalSizeFor(ContentSizeLogical, Renderer.GetDpiScaleFactor());
    if (Physical.X != SceneTextureSizePhysical.X || Physical.Y != SceneTextureSizePhysical.Y) {
        RecreateSceneTexture(Renderer);
    }

    // 2. Zero size, or creation failed → nothing to render or composite this frame.
    if (!SceneTexture) {
        return;
    }

    SDL_Renderer* Sdl = Renderer.GetSdlRenderer();

    // 3. Save the active SDL clip so the target switch can restore it afterwards.
    SDL_Rect   SavedClip{};
    const bool ClipWasEnabled = SDL_RenderClipEnabled(Sdl);
    if (ClipWasEnabled) {
        SDL_GetRenderClipRect(Sdl, &SavedClip);
    }

    // 4-5. Redirect drawing to the scene texture; the whole texture is writable.
    SDL_SetRenderTarget(Sdl, SceneTexture);
    SDL_SetRenderClipRect(Sdl, nullptr);

    // 6. Clear the texture to the consumer's scene background colour.
    SDL_SetRenderDrawColor(Sdl, SceneClearColor.R, SceneClearColor.G, SceneClearColor.B,
                           SceneClearColor.A);
    SDL_RenderClear(Sdl);

    // 7. Let the consumer draw the scene through the ordinary Renderer interface.
    //    It receives the logical content size; the Renderer's DPI scaling maps its
    //    logical draw calls onto the physical-pixel texture automatically.
    if (OnRenderScene) {
        OnRenderScene(Renderer, ContentSizeLogical);
    }

    // 8-9. Back to the screen, and restore the clip the UI pass was using.
    SDL_SetRenderTarget(Sdl, nullptr);
    SDL_SetRenderClipRect(Sdl, ClipWasEnabled ? &SavedClip : nullptr);

    // 10. Composite the finished scene into the content rect in the normal UI pass.
    Renderer.DrawTexture(SceneTexture, ContentRect);
}

} // namespace Penumbra::Widgets
