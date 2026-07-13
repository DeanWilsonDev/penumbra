# Penumbra — Glow/Gradient Fidelity Requirements for Pharos

> **Scope:** Two related polish gaps found while building the Atlas
> "lighthouse haze" motif (pulsing selection/hover glow, drifting mist
> overlay, per-tile liquid shimmer) in `src/ui/atlas_panel.cpp`, tested
> against `penumbra-proto` commit `112c6b4962ea34313add511d2db4a26b384c4861`
> (the commit `cmake/Dependencies.cmake` currently pins).
> **Status:** Not blocking. Everything described below shipped in Pharos
> using only `Renderer::DrawDropShadow` and `Renderer::DrawGradientRect` as
> they exist today — composition got the effect on screen. This doc is
> asking for two primitives that would let Pharos stop leaning on
> `DrawDropShadow` for something its own contract wasn't written for, and
> would raise the visual ceiling on glow/liquid effects generally.
> **Not in scope:** anything achievable by tuning colors/alpha/timing in
> Pharos's own code — that's exactly what the current implementation
> already does, and where it hit a wall is the boundary this doc describes.

---

## 0. Context

**What Pharos built:** three effects that all want a soft, animated,
roughly-circular light source:

- A pulsing accent-colored glow behind the selected/hovered treemap tile.
- Four drifting "mist" blobs layered over the whole treemap.
- A small drifting specular highlight ("glint") on every large-enough leaf
  tile, plus a top-lit gradient fill, evoking a glassy liquid surface (the
  motivating reference was the health/mana orb liquid in recent Diablo
  titles — a deep saturated base color with a bright moving highlight).

All three currently reuse `Renderer::DrawDropShadow`
(`src/Penumbra/Render/Renderer.cpp:297`):

```cpp
void Renderer::DrawDropShadow(Rect RectLogical, Color ShadowColor, float BlurRadiusLogical,
                              float CornerRadiusLogical) {
    ...
    // The inner ring sits exactly on the caster's own edge (solid alpha); the widget
    // itself is drawn on top afterwards and covers the interior, so no centre fill is
    // needed here -- just the ring, fading outward over Blur instead of the fixed 1px
    // AA feather.
    const int Segments = SegmentsForRadius(Radius + Blur);
    const RoundedRing Ring = BuildRoundedRing(R, Radius, Segments);
    ...
```

Pharos passes a near-zero-size (2x2 logical px) caster rect and a large
`BlurRadiusLogical`, which turns the "solid at the caster's edge, fading to
transparent `Blur` pixels out" ring into a usable soft circular glow — a
poor-man's radial gradient. It works (see `drawPulsingGlow`, `drawMistHaze`,
`drawLiquidShimmer` in `src/ui/atlas_panel.cpp`), but it's a workaround:

- **The falloff is fixed and linear.** `DrawDropShadow`'s contract is "solid
  core, straight-line fade to zero over `Blur`" — there's no way to ask for
  a softer (eased) falloff, or a tighter hot core with a longer tail, both
  of which a real light/liquid highlight wants.
- **It's single-stop.** The ring interpolates one color to transparent.
  A liquid specular highlight or a richer glow (bright white core → mid
  accent color → transparent edge) needs multiple stops in one draw; today
  that means stacking multiple `DrawDropShadow` calls at different radii,
  which Pharos does not currently do (tuning it well enough with one stop
  was judged good enough for now, not because a second stack wouldn't help).
- **Segment count is sized for a *ring* geometry, not a filled disc.**
  `SegmentsForRadius(Radius + Blur)` (`Renderer.cpp:27`) scales with the
  *combined* corner radius and blur, which is correct for a thin AA-fringed
  ring but means Pharos's near-zero-caster trick pays for the same
  tessellation cost as a much larger true shadow, for a shape that's
  conceptually a filled circle, not a ring.

Separately, **there is no blend-mode control anywhere in `Renderer`**.
`Renderer::Initialise` sets it once, globally, at startup and nothing ever
changes it:

```cpp
// src/Penumbra/Render/Renderer.cpp:68-80
bool Renderer::Initialise(SDL_Renderer* InSdlRenderer, float InDpiScaleFactor,
                          IFontBackend* InFontBackend) {
    if (!InSdlRenderer) {
        return false;
    }
    SdlRenderer = InSdlRenderer;
    DpiScaleFactor = (InDpiScaleFactor > 0.0f) ? InDpiScaleFactor : 1.0f;
    FontBackend = InFontBackend;
    // Alpha blending is required for the anti-aliasing fringe (zero-alpha vertices)
    // to fade against what is behind it.
    SDL_SetRenderDrawBlendMode(SdlRenderer, SDL_BLENDMODE_BLEND);
    return true;
}
```

(`ViewportWidget.cpp:87` separately hardcodes `SDL_BLENDMODE_BLEND` on the
scene texture itself, same story.) A full search of `include/` and `src/`
for `BlendMode` turns up only these two call sites — no public API exists to
change it per draw call. Every glow, mist blob, and shimmer highlight Pharos
draws therefore composites via standard "over" alpha blending. `drawMistHaze`
gets away with this because its four blobs are deliberately kept low-alpha
and rarely overlap much, but overlapping translucent light sources should
get *brighter*, not just more opaque — that's what additive blending
(`SDL_BLENDMODE_ADD`, natively supported by SDL3) gives for free, and normal
"over" blending structurally cannot: it approaches the source color
asymptotically as layers stack, it never brightens past it.

---

## 1. REQUIRED — a real radial/multi-stop gradient primitive

### Proposed API

```cpp
// include/Penumbra/Render/Renderer.h, alongside DrawGradientRect

// A radial gradient centred at Centre, solid CentreColor at the middle,
// interpolating outward to EdgeColor at RadiusLogical. Same vertex-color
// interpolation technique DrawGradientRect already uses (a solid-centre fan
// plus a fading outer ring), just keyed by radial distance instead of Y
// position, and independent of any caster rect -- no more borrowing
// DrawDropShadow's ring-around-a-shape contract for something that wants a
// filled disc.
void DrawRadialGradient(Point Centre, float RadiusLogical, Color CentreColor, Color EdgeColor);
```

`DrawGradientRect` (`Renderer.cpp:149`) already builds almost exactly this
shape for its rounded-corner path — a solid center vertex, an inner ring,
an outer ring fading to alpha zero, `SDL_RenderGeometry` with 1 + 2*Count
vertices. `DrawRadialGradient` is the same structure with `LerpAtY` replaced
by distance-from-`Centre`, and no rect/corner-radius parameters to reconcile
with — it would likely share most of its vertex-generation code with
`DrawGradientRect`'s rounded-corner branch via a small internal helper.

A multi-stop variant (`std::vector<GradientStop>` of `{float T; Color C;}`)
would be a nice-to-have on top of this but isn't required — Pharos's actual
uses (a bright core fading to one edge color) are all 2-stop.

### What unblocks in Pharos

- `drawPulsingGlow`, `drawMistHaze`, and `drawLiquidShimmer` in
  `src/ui/atlas_panel.cpp` all replace their near-zero-caster
  `DrawDropShadow` call with one `DrawRadialGradient` call — same visual
  role, correct semantics, and (per the segment-count point in §0) cheaper
  per call since the tessellation is sized for the actual shape being drawn.
- The liquid-shimmer highlight (currently a single-stop white blob) becomes
  able to do a proper "hot white core → tinted accent → transparent" glint
  in one draw call instead of needing to fake it with stacked
  `DrawDropShadow` calls at different radii.

---

## 2. REQUIRED — a way to draw with additive blending

### Proposed API

```cpp
// include/Penumbra/Render/Renderer.h

enum class BlendMode { Normal, Additive };

// Scoped like the clip-rect stack (PushClipRect/PopClipRect) -- every push
// must be matched by a pop, nesting is not expected (SDL's blend mode is a
// single current state, not a stack of composited layers), but a stack
// keeps the call symmetric with the clip API Pharos already knows and makes
// "did I forget to restore this" a debug-assertable invariant the same way
// unbalanced clip pushes already are.
void PushBlendMode(BlendMode Mode);
void PopBlendMode();
```

Implementation is a thin wrapper: `PushBlendMode(Additive)` calls
`SDL_SetRenderDrawBlendMode(SdlRenderer, SDL_BLENDMODE_ADD)`,
`PopBlendMode()` restores `SDL_BLENDMODE_BLEND`. `SDL_BLENDMODE_ADD` is a
built-in SDL3 blend mode (no custom `SDL_ComposeCustomBlendMode` needed for
the first cut) so this is a small, self-contained change confined to
`Renderer.cpp`.

### What unblocks in Pharos

`drawMistHaze`'s four blobs, and the selected/hovered glow when it happens
to overlap a mist blob, get wrapped in `PushBlendMode(Additive)` /
`PopBlendMode()`. Overlapping light reads as brighter/hotter where it
overlaps, the way real light does, instead of Pharos having to keep every
glow's alpha low enough that the difference between additive and "over"
compositing stays invisible (which is the only reason today's values look
acceptable — they're tuned around this limitation, not past it).

---

## 3. Explicitly not requested

- **A full shader/postprocess hook.** A true liquid-surface shader
  (refraction, dynamic meniscus shape, real specular falloff curves) is a
  much bigger ask than either item above and isn't what's blocking Pharos
  right now — the two primitives above get the current motif most of the
  way there. If Pharos ever wants genuine liquid physics rather than a
  glint-and-gradient approximation, that's a separate, later conversation.
- **Multi-stop gradients as a hard requirement.** Noted as a nice-to-have
  in §1, but every current Pharos use case is 2-stop; don't block on it.
- **Custom/arbitrary blend modes** (`SDL_ComposeCustomBlendMode`). Additive
  is the only mode any current Pharos effect wants; a general blend-mode
  composer is more API surface than today's ask justifies.
- **Changing what `DrawDropShadow` itself does.** It's correct and used
  correctly elsewhere (panel elevation shadows, the Atlas tooltip). This doc
  is asking for a sibling primitive, not a change to its existing contract.

---

## 4. What unblocks when this lands

`src/ui/atlas_panel.cpp`'s glow/haze/shimmer helpers stop repurposing a
shadow primitive for something it wasn't designed for, become cheaper per
draw call, and — once additive blending is available — overlapping mist and
glow start reading as genuinely brighter light rather than just more opaque
color. The liquid-shimmer tile highlight can gain a real multi-tone glint
instead of a flat-color blob, closing most of the remaining visual gap
between it and the Diablo orb reference that motivated it.
