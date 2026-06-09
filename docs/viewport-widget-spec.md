# ViewportWidget — Requirements

> **Scope:** A single new widget added to the existing Penumbra PoC.
> **Purpose:** The rendering seam between Penumbra's UI pass and Dawn's scene pass.
> **Hard constraint:** Dawn code never calls an SDL function. All SDL render-target
> management is locked inside `ViewportWidget`.

---

## 1. What It Is

`ViewportWidget` is a `Box` subclass. From Penumbra's perspective it is just another
widget in the tree: it has a position and size, a `BoxStyle` (border, padding, background),
and participates in the normal two-phase layout. What makes it special is what happens
in its `DrawContent`: it redirects the SDL render target to an off-screen texture, invokes
a Dawn-provided callback to draw the scene, then restores the render target and blits the
texture into the content rect in the normal UI pass.

Dawn's scene drawing code uses the same `Render::Renderer` interface as everything else.
It calls `DrawFilledRect`, `DrawText`, etc. It never calls SDL. Penumbra switches the
target before the callback and restores it after — Dawn does not know this is happening.

```
Normal UI draw pass
    ...
    ViewportWidget::DrawContent()
        SDL_SetRenderTarget → scene texture      ← inside Penumbra, invisible to Dawn
        SDL_RenderClear
        OnRenderScene(Renderer, SceneSizeLogical) ← Dawn draws here via Renderer interface
        SDL_SetRenderTarget → screen             ← inside Penumbra
        blit scene texture into ContentRect
    ...
```

---

## 2. Required Change to `Renderer`

`ViewportWidget` needs to create and destroy an SDL texture, switch render targets, and
blit the result. These are SDL calls. To keep SDL locked inside the Render layer,
`Renderer` needs two small additions:

```cpp
// In Renderer.h / Renderer.cpp

// Returns the underlying SDL_Renderer so ViewportWidget can manage its texture.
// Use only in Penumbra internals; Dawn never calls this.
SDL_Renderer* GetSdlRenderer() const;

// Draws a previously acquired SDL_Texture into a logical-pixel destination rect.
// Used by ViewportWidget to composite the scene texture back into the UI pass.
void DrawTexture(SDL_Texture* Texture, SDL_FRect DestLogical);
```

No other changes to `Renderer`.

---

## 3. Public API

File: `include/Penumbra/Widgets/ViewportWidget.h`

```cpp
namespace Penumbra::Widgets {

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
    // No default — the consumer sets this. Transparent by default (zero-init BoxStyle).
    SDL_Color SceneClearColor{0, 0, 0, 255};

    SDL_FPoint Measure(SDL_FPoint AvailableSizeLogical) override;
    void       Arrange(SDL_FRect FinalRectLogical) override;
    bool       UpdateInteractionState(const Platform::InputState&) override;
    void       Draw(Render::Renderer&) override;

    ~ViewportWidget() override;

private:
    void RecreateSceneTexture(Render::Renderer&);

    SDL_Texture* SceneTexture{nullptr};
    SDL_FPoint   SceneTextureSizePhysical{0.0f, 0.0f}; // tracks current texture size
    SDL_FPoint   ContentSizeLogical{0.0f, 0.0f};       // from last Arrange
};

} // namespace Penumbra::Widgets
```

---

## 4. Texture Lifecycle

- The texture is created (or recreated) in `Draw`, lazily, the first time and whenever
  `ContentSizeLogical` changes from the previous frame. Arrange updates `ContentSizeLogical`;
  Draw compares against `SceneTextureSizePhysical` to detect a size change.
- Size at creation: `(ContentSizeLogical.x * DpiScale, ContentSizeLogical.y * DpiScale)`
  rounded to the nearest integer, in physical pixels.
- Format: `SDL_PIXELFORMAT_RGBA8888`, access `SDL_TEXTUREACCESS_TARGET`.
- The old texture is destroyed before the new one is created.
- Destroyed in the destructor.
- If the content size is zero (widget not yet arranged, or collapsed), skip creation and
  skip the `OnRenderScene` call for that frame.

---

## 5. Draw Sequence (inside `DrawContent`)

```
1. If ContentSizeLogical changed since last frame → RecreateSceneTexture(Renderer)
2. If SceneTexture is null → return (zero-size or failed creation)
3. Save the active SDL clip rect  (SDL_GetRenderClipRect)
4. SDL_SetRenderTarget(SdlRenderer, SceneTexture)
5. SDL_SetRenderClipRect(SdlRenderer, nullptr)   // full texture is writable
6. SDL_SetRenderDrawColor + SDL_RenderClear      // clear to SceneClearColor
7. if OnRenderScene → invoke OnRenderScene(Renderer, ContentSizeLogical)
8. SDL_SetRenderTarget(SdlRenderer, nullptr)     // back to the screen
9. Restore the saved SDL clip rect
10. Renderer.DrawTexture(SceneTexture, ContentRect)
```

Steps 4 and 8 use `Renderer.GetSdlRenderer()` to access the underlying SDL_Renderer.
Everything else goes through the `Renderer` interface.

---

## 6. DPI

- The texture is physical-pixel sized (content × dpiScale). This matches how SDL_ttf
  rasterises glyphs — crisp at any density.
- `OnRenderScene` receives `ContentSizeLogical` (logical pixels). If Dawn's scene drawing
  code uses `Render::Renderer` draw calls, the Renderer's DPI scaling applies automatically,
  the same as it does for UI widgets.
- Dawn should not query SDL for screen dimensions or DPI inside the callback. Everything
  it needs is in `ContentSizeLogical`.

---

## 7. Input Routing (inside `UpdateInteractionState`)

```
1. If mouse is outside ArrangedRect → return false (no consumption)
2. Walk Children in reverse order (front-to-back, same as Box)
   → if any child consumes the input → return true
3. If mouse is inside ContentRect (the clipped scene area):
   → if OnSceneInput → invoke and return its result
4. Return true  (being over the viewport is itself opaque — prevents fall-through
                  to whatever is behind it in the tree)
```

Children are UI overlays rendered on top of the scene (e.g. a toolbar, a gizmo panel).
They get first refusal. If none of them want the event and the mouse is inside the scene
area, Dawn handles it. Input outside the border/padding area of the viewport is not
forwarded to Dawn.

---

## 8. CMakeLists

Add `src/Penumbra/Widgets/ViewportWidget.cpp` to the `penumbra` target source list.
No new dependencies.

---

## 9. Demo Wiring (to verify it works)

Add a `ViewportWidget` to `demo/main.cpp` alongside the existing settings panel to prove
the full slice. A minimal `OnRenderScene` that draws a checkerboard of `DrawFilledRect`
calls in two alternating colors is enough — it proves the render-target switch works,
the DPI is correct, and the blit composites cleanly over the UI background.

The demo does not need to model a level or scene objects. The checkerboard is sufficient
proof for this step.

---

## 10. Deferred

- **Camera / coordinate transform.** Dawn will need pan + zoom (a 2D camera matrix).
  For this PoC the callback draws at world coords = content coords (no transform). Dawn
  owns the camera state and applies it inside `OnRenderScene` when ready.
- **Sprite / image drawing.** `Render::Renderer` will need a `DrawSprite` or
  `DrawTexturedRect` method to render tile and entity textures. Out of scope here.
- **Input coordinate transform.** Once a camera exists, Dawn will need to unproject the
  screen-space mouse position into world space. `ContentRect` (passed to `OnSceneInput`)
  gives the origin; the camera transform is Dawn's concern.
- **Resize performance.** The texture is destroyed and recreated on every size change.
  For a draggable panel splitter this is acceptable. Double-buffering the texture during
  resize is a later optimisation.
