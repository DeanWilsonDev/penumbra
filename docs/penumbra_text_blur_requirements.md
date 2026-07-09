# Penumbra — Text/Scene Sharpness Requirements for Pharos

> **Scope:** A rendering-quality gap found after `docs/penumbra_dpi_requirements.md`
> landed, tested against `penumbra-proto` commit `a12663b`.
> **Status:** Not blocking (nothing is broken/unusable), but a real, visible
> quality regression from Dear ImGui: text in the Atlas panel and the Overlay
> selector reads noticeably soft/blurry, most visible at small font sizes.
> **Not in scope:** anything achievable in Pharos's own code — the two root
> causes below are both inside `Renderer`/`ViewportWidget`'s blit math; Pharos
> only ever supplies logical positions and has no way to intervene between
> that and the final GPU blit.

---

## 0. Context

**Symptom:** text in the Atlas panel (treemap labels, tooltip) and the
Overlay selector ("Color by:" panel) looks visibly blurrier than the same
text in Explorer/Inspector, especially at `theme.FontSizeSmall` (10pt, used
by Atlas). All panels are affected to *some* degree — Atlas and Overlay are
just where it's most visible.

Two separate, compounding causes, both traced to exact lines in
`penumbra-proto`'s current source:

### Cause 1 — glyph blits aren't pixel-snapped (affects every panel)

```cpp
// src/Penumbra/Render/Renderer.cpp:242-244
const SDL_FRect Destination{PositionLogical.X * DpiScaleFactor,
                            PositionLogical.Y * DpiScaleFactor, TextureWidth, TextureHeight};
SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
```

`Destination.x`/`.y` is a raw `float` multiplication with no rounding.
`TextureWidth`/`TextureHeight` (the destination's *size*) come from the
already-rasterized glyph texture, so the blit is 1:1 in *scale* — but its
*position* lands on whatever sub-pixel offset `PositionLogical * DpiScaleFactor`
happens to produce, which is essentially never a whole physical pixel (text
positions come from cumulative layout arithmetic — row heights, padding,
nested `SplitPanel` ratios applied to an arbitrary window size — none of
which round to integers).

SDL3's default texture scale mode is `SDL_SCALEMODE_LINEAR` (bilinear) —
confirmed in the installed SDL3 headers, and nothing in `Renderer` or
`SdlTtfFontBackend` ever calls `SDL_SetTextureScaleMode` to change it. A
texture blitted at a fractional pixel offset under linear filtering samples
across texel boundaries, which is exactly what reads as "soft" or "blurry"
text — the glyph itself is rasterized crisp (physical-pixel-sized, DPI-correct),
it's the *positioning* that softens it on the way to the screen.

### Cause 2 — `ViewportWidget`'s scene texture size doesn't match its own blit destination (Atlas only, whole scene, not just text)

```cpp
// src/Penumbra/Widgets/ViewportWidget.cpp:16-18
Point PhysicalSizeFor(Point ContentSizeLogical, float DpiScale) {
    return {std::round(ContentSizeLogical.X * DpiScale),
            std::round(ContentSizeLogical.Y * DpiScale)};
}
```

The scene texture itself is created at this **rounded** physical size
(`RecreateSceneTexture`, `ViewportWidget.cpp:73-83`). But the blit that
composites it back into the UI pass goes through the same `DrawTexture` →
`ToPhysical` path as everything else:

```cpp
// src/Penumbra/Render/Renderer.cpp:251-259
void Renderer::DrawTexture(SDL_Texture* Texture, Rect DestLogical) {
    ...
    // The scene texture is sized in physical pixels (content × dpi); the logical
    // destination scales to the same physical extent, so the blit is 1:1 — no resample.
    const SDL_FRect Destination = ToPhysical(DestLogical);
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}
```

`ToPhysical` is an **unrounded** `RectLogical * DpiScaleFactor`. The comment
above it asserts the blit is 1:1, but that's only true when
`ContentSizeLogical.X * DpiScale` is already a whole number — whenever it
isn't (the common case), the texture's actual pixel dimensions (rounded) and
the blit destination's size (unrounded) differ by a fraction of a pixel, and
`SDL_RenderTexture` scales the *entire* texture — not just text, every
treemap rect and border drawn inside it — by that tiny factor. Under linear
filtering, this softens the whole Atlas scene, compounding with Cause 1 for
any text drawn inside it (the treemap labels, the tooltip).

---

## 1. REQUIRED — snap glyph blit position to the nearest physical pixel

### Proposed fix

```cpp
// src/Penumbra/Render/Renderer.cpp
void Renderer::DrawText(FontHandle Font, std::string_view Text, Point PositionLogical,
                        Color InColor) {
    ...
    const SDL_FRect Destination{std::round(PositionLogical.X * DpiScaleFactor),
                                std::round(PositionLogical.Y * DpiScaleFactor),
                                TextureWidth, TextureHeight};
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}
```

`TextureWidth`/`TextureHeight` are already whole physical pixels (the glyph
texture's own dimensions), so only the position needs rounding — the blit
becomes a true 1:1 pixel copy, no filtering involved regardless of scale mode.

(Whether `DrawFilledRect`/`DrawRectOutline` want the same treatment for their
non-rounded-corner fast path is a judgment call for whoever owns the rounded-
corner geometry code — flagging it here since the same sub-pixel-position
softening would apply to any hard-edged rect, but it wasn't what was reported
so this doc doesn't ask for it.)

---

## 2. REQUIRED — make `ViewportWidget`'s scene-texture blit genuinely 1:1

The comment already documents the intent ("no resample") — the fix is making
the code match it.

### Proposed fix

Round the destination the same way the texture's own size was rounded, so
the two agree exactly:

```cpp
// src/Penumbra/Render/Renderer.cpp
SDL_FRect Renderer::ToPhysical(Rect RectLogical) const {
    return {RectLogical.X * DpiScaleFactor, RectLogical.Y * DpiScaleFactor,
            RectLogical.W * DpiScaleFactor, RectLogical.H * DpiScaleFactor};
}
```

`ToPhysical` is used by more than just `DrawTexture` (clip rects, filled
rects), so rounding it globally is probably too broad a change for this ask.
Narrower alternative — round only in `DrawTexture`, which is the one caller
that needs its destination size to match a texture's actual (rounded) pixel
dimensions exactly:

```cpp
// src/Penumbra/Render/Renderer.cpp
void Renderer::DrawTexture(SDL_Texture* Texture, Rect DestLogical) {
    if (!Texture) return;
    SDL_FRect Destination = ToPhysical(DestLogical);
    Destination.w = std::round(Destination.w);
    Destination.h = std::round(Destination.h);
    SDL_RenderTexture(SdlRenderer, Texture, nullptr, &Destination);
}
```

This doc doesn't have a strong opinion on exactly where the rounding lives
(`DrawTexture` itself vs. a variant of `ToPhysical` vs. something in
`ViewportWidget`) — only that `ViewportWidget`'s scene texture and its own
blit destination should agree to the pixel, since the code already assumes
they do.

---

## 3. Explicitly not requested

- **Rounding for `DrawFilledRect`/`DrawRectOutline`'s straight-edge fast path.**
  Noted as a related, likely-present softening under the same mechanism, but
  not what was reported — revisit only if it turns out to matter visually.
- **Changing SDL3's default texture scale mode** (e.g. calling
  `SDL_SetTextureScaleMode(..., SDL_SCALEMODE_NEAREST)` globally). Linear
  filtering is correct and wanted for the rounded-corner geometry path
  (`SegmentsForRadius` / `BuildRoundedRing`) and for any future texture
  content (sprites) — the fix belongs at the position/size level (items 1–2),
  not by turning off filtering everywhere.

---

## 4. What unblocks when this lands

Once items 1–2 land, Atlas and Overlay text renders exactly as crisp as
Explorer/Inspector's already does, and the Atlas scene's rects/borders lose
the faint softness from the scene-texture scale mismatch too.
