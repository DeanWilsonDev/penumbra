# Penumbra — Requirements for Pharos

> **Scope:** Capability gaps in Penumbra (as of `penumbra` commit `8f2682e`,
> "add viewport component") found while porting Pharos off Dear ImGui.
> **Status:** Blocking. Pharos's panel layer is stubbed out (see `src/ui/*_panel.cpp`)
> pending these additions.
> **Not in scope:** anything achievable by writing more Pharos application code
> against Penumbra's existing API. Only genuine framework gaps are listed here.

---

## 0. Context

Pharos's four panels (Explorer, Atlas, Inspector, Overlay) were built against
ImGui's docking branch: a fixed split of the window into four regions
(Explorer left 20%, Inspector right 25%, Overlay bottom 30% of the remaining
space, Atlas fills the rest), each region user-resizable by dragging the
border between panes. `imgui.ini` persisted the layout, though pane
*resizing* — not persistence — is the load-bearing behavior; nothing in
Pharos depends on drag-to-rearrange tabs or floating/undockable windows.

Everything else Pharos needs maps cleanly onto Penumbra's existing widget
catalogue and was portable without a framework change:

| Pharos need | Penumbra mechanism |
| --- | --- |
| Atlas panel's per-frame data-driven nested-rect drawing + hover/click hit-testing | `ViewportWidget::OnRenderScene` / `OnSceneInput`, drawing directly via `Renderer` |
| Hover tooltip near the cursor | Drawn manually inside the same `OnRenderScene` callback — no separate popup layer needed |
| Explorer tree rows, Inspector metric grid | Composition: nested `Box` with `LayoutMode::VerticalStack` / `HorizontalStack`, subclassing `Box` for fixed-width columns via `MeasureContent` overrides (the documented extension point) |
| Overlay radio group | `Checkbox` widgets with app-level mutual exclusion in `OnChanged` |
| File-path entry | `TextInput` |

Five things did not have a Penumbra-side answer, listed below. A sixth,
much smaller pair of items is included because there's no workaround
available for them within the architecture's own rules.

---

## 1. REQUIRED — a resizable two-pane split widget

Penumbra's only container primitives are `Box` (children sized by their own
`Measure()`, no relative/flex sizing on the main axis) and `ScrollablePanel`.
Neither supports a user-draggable divider between two regions. The demo
(`demo/main.cpp`) computes its two-pane layout by hand-splitting a rect with a
hardcoded ratio outside the widget tree entirely — workable for a fixed
split, but Pharos's panes must be resizable at runtime, and drag-to-resize is
exactly the kind of stateful, input-consuming interaction Penumbra already
builds as first-class widgets elsewhere (`NumericDrag`, `TextInput`). Hand-
rolling mouse-capture-drag logic in Pharos's application code would duplicate
that idiom outside the framework — the thing this document exists to avoid.

Three nested instances of this one widget reconstruct Pharos's whole layout
(outer horizontal split for Explorer | rest, a second horizontal split for
rest | Inspector, a vertical split for Atlas | Overlay). No tab/dock-group
concept is needed — no two Pharos panels ever share a region.

### Proposed API

File: `include/Penumbra/Widgets/SplitPanel.h`

```cpp
namespace Penumbra::Widgets {

enum class SplitAxis { Horizontal, Vertical }; // Horizontal = divider runs vertically, splits left/right

struct SplitPanelStyle : BoxStyle {
    Color ColorHandle{0, 0, 0, 0};
    Color ColorHandleHovered{0, 0, 0, 0};
    Color ColorHandleDragged{0, 0, 0, 0};
};

// Exactly two children, sized by a draggable ratio rather than their own
// Measure(). Mirrors NumericDrag's click-and-drag idiom (internal Dragging
// bool + last-pointer-position, consuming input regardless of hover once a
// drag has started) but drives a split ratio instead of a scalar value.
class SplitPanel : public Box {
public:
    SplitAxis Axis{SplitAxis::Horizontal};
    float     SplitRatio{0.5f};           // 0..1, fraction given to the first child
    float     HandleThicknessLogical{0.0f};
    float     MinPaneSizeLogical{0.0f};   // clamps SplitRatio so neither pane collapses

    // First/Second replace Box::Children for this widget's two slots; Box's
    // generic Children vector is unused here (AddChild is not meaningful).
    WidgetBase* SetFirst(std::unique_ptr<WidgetBase> Child);
    WidgetBase* SetSecond(std::unique_ptr<WidgetBase> Child);

    void ApplyStyle(const SplitPanelStyle& Style);

    Point Measure(Point AvailableSizeLogical) override;
    void  Arrange(Rect FinalRectLogical) override;
    bool  UpdateInteractionState(const Platform::InputState&) override;
    void  Draw(Render::Renderer&) override;

private:
    std::unique_ptr<WidgetBase> First;
    std::unique_ptr<WidgetBase> Second;
    Rect                        HandleRect{};
    bool                        Dragging{false};
    float                       LastPointerMain{0.0f}; // x for Horizontal, y for Vertical
};

} // namespace Penumbra::Widgets
```

(`Color` and `Point`/`Rect` here are the types requested in items 3 and 4
below, not `SDL_Color`/`SDL_FPoint`/`SDL_FRect` — no new widget should be
adding another SDL-typed field to the public API.)

### Behavior

- **Measure:** returns `AvailableSizeLogical` — like `ViewportWidget`, a split
  is greedy; the parent's layout decides its bounds.
- **Arrange:** splits `FinalRectLogical` along `Axis` at `SplitRatio`, minus
  `HandleThicknessLogical` for the divider itself; clamps `SplitRatio` so each
  side keeps at least `MinPaneSizeLogical`. Calls `First->Arrange(...)` /
  `Second->Arrange(...)` with the two resulting rects. Each child still gets
  its own `Measure()` call first (as `Box::Arrange` does for its children)
  so, e.g., a `ScrollablePanel` inside a pane behaves normally.
- **UpdateInteractionState:** press-on-handle starts a drag (same shape as
  `NumericDrag::UpdateInteractionState`); while dragging, pointer delta along
  the main axis (`x` for `Horizontal`, `y` for `Vertical`) adjusts
  `SplitRatio` by `delta / (mainAxisExtent)`, independent of current hover.
  Release ends the drag. Then delegates to `First`/`Second` (reverse order,
  same first-refusal rule as `Box`), returning `true` if either consumed the
  input, else `true` while dragging, else `Hovered` over the handle.
- **Draw:** draws `First`, `Second`, then the handle rect on top (hover/drag
  color per `SplitPanelStyle`) so it's always grabbable even if a child's
  background would otherwise cover it.
- No persistence. Pharos will own saving/restoring `SplitRatio` values itself
  if that turns out to matter later — out of scope for this ask.

---

## 2. REQUIRED — a widget visibility flag

`WidgetBase` has no notion of "present in the tree but not currently shown."
`Box::Measure`, `Arrange`, `UpdateInteractionState`, and `Draw` all walk
every entry in `Children` unconditionally.

Pharos's Explorer panel is a tree view built once as a retained widget tree
(one row per `TreeNode`, mirroring the app's data hierarchy) with expand/
collapse toggled by the user at runtime. Without a visibility flag, toggling
a branch closed means removing its subtree's widgets from the parent's
`Children` vector, and reopening it means rebuilding that subtree from
scratch — losing any per-widget state (e.g., a nested collapsed branch three
levels down forgets it was collapsed) and doing real allocation work on every
click. That's a workaround, not a fix, for something the framework should
own: a collapsed branch should just contribute zero size and skip drawing
and hit-testing, the same way `display: none` does.

### Proposed API

`include/Penumbra/Widgets/WidgetBase.h`:

```cpp
void SetIsVisible(bool Visible) { IsVisible = Visible; }
bool GetIsVisible() const { return IsVisible; }

protected:
bool IsVisible{true};
```

`src/Penumbra/Widgets/Box.cpp` — `Measure`, `Arrange`, `UpdateInteractionState`,
and `Draw` each skip a child where `!Child->GetIsVisible()`:

- **Measure:** contributes `{0, 0}` to the stack total (no gap counted on its
  side either — treat it as absent, not zero-sized-but-present).
- **Arrange:** not called at all (an un-arranged widget keeps its last
  `ArrangedRect`, which is fine since it's never drawn or hit-tested either).
- **UpdateInteractionState:** skipped, so a hidden subtree can't consume
  input.
- **Draw:** skipped.

This is a small, generically useful primitive (any future collapsible
section wants it, not just Pharos's tree view) rather than a Pharos-specific
special case.

---

## 3. REQUIRED — a Penumbra-owned `Color` type

Every styleable color slot in Penumbra's public API (`BoxStyle::ColorBackground`,
`ButtonStyle::ColorBackgroundHovered`, `TextInput::ColorCaret`,
`ViewportWidget::SceneClearColor`, every `Renderer::Draw*` call, …) is typed as
`SDL_Color`. That means any consumer — Pharos, eventually Dawn — has to
`#include <SDL3/SDL.h>` just to construct or store a color, which is exactly
the boundary the architecture doc says Penumbra exists to hold: *"the only
code that calls SDL directly is `Penumbra::Platform`"*. A consumer reaching
for an SDL header to build a `BoxStyle` is that rule leaking through
`Penumbra::Widgets` and `Penumbra::Render` instead of stopping at
`Penumbra::Platform`.

This isn't hypothetical for Pharos — it already happened. `src/metrics/color_map.h`
computes the treemap's overlay gradients (a pure data-to-color mapping with no
rendering in it) and, to hand the result to `Renderer::DrawFilledRect`, it
currently does:

```cpp
// src/metrics/color_map.h — what Pharos has today, and wants to remove
#include <SDL3/SDL.h>
...
SDL_Color metricToColor(std::optional<double> value, double minVal, double maxVal, Overlay overlay);
```

That's an SDL include in a file that has nothing to do with SDL, purely
because `SDL_Color` is the only color type in scope. It should be
`Penumbra::Render::Color` (or wherever this type ends up living), with no SDL
header in sight.

### Proposed API

File: `include/Penumbra/Render/Color.h`

```cpp
namespace Penumbra::Render {

// Penumbra's own color type. Same layout as SDL_Color, but owned by Penumbra
// so nothing above the Platform layer needs an SDL header to spell a color.
struct Color {
    std::uint8_t R{0};
    std::uint8_t G{0};
    std::uint8_t B{0};
    std::uint8_t A{0};
};

} // namespace Penumbra::Render
```

### Required changes elsewhere

- `Styles.h`: `BoxStyle`, `ButtonStyle`, `CheckboxStyle` (and the proposed
  `SplitPanelStyle` in item 1) switch every `SDL_Color` field to `Color`.
- `TextInput`, `NumericDrag`, `Label`, `ViewportWidget`: same, for
  `ColorText` / `ColorCaret` / `ColorSelection` / `SceneClearColor` etc.
- `Renderer`: `BeginFrame`, `DrawFilledRect`, `DrawRectOutline`, `DrawText`
  take `Color` instead of `SDL_Color`. `Renderer.cpp` converts `Color` →
  `SDL_Color` internally at the point it calls into SDL — that conversion is
  an implementation detail of the Render layer, not something a consumer
  should ever do.
- `InputState`'s color-typed fields (there are none today) are covered by
  this item as a matter of course; its geometry (`MousePosition`) and key/
  modifier fields are separate types, covered by items 4 and 5 below rather
  than here.

### What unblocks in Pharos

`src/metrics/color_map.h`/`.cpp` drop `#include <SDL3/SDL.h>` and use
`Penumbra::Render::Color` in its place — the only reason that include exists
today is the absence of this type.

---

## 4. REQUIRED — Penumbra-owned geometry types (`Point`, `Rect`)

Same shape of problem as `Color`, and the same principle applies: Penumbra's
whole layout and drawing vocabulary — `WidgetBase::Measure`/`Arrange`,
`Box::MeasureContent`/`DrawContent`, every `Renderer::Draw*` call,
`InputState::MousePosition` — is typed in `SDL_FPoint` and `SDL_FRect`. This
one is *more* unavoidable for Pharos than `Color` was, not less: the doc's
own section 0 says Pharos will port the Inspector grid and Explorer tree rows
by "subclassing `Box` for fixed-width columns via `MeasureContent`
overrides." That override's signature is fixed by the base class —
`SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) override` — so
Pharos is required to spell an SDL type in its own header just to satisfy a
virtual override, with no way to avoid it locally. Same for any custom
`DrawContent(Render::Renderer&, SDL_FRect ContentRect) override`.

### Proposed API

File: `include/Penumbra/Geometry.h`, at the root `Penumbra::` namespace —
below `Platform`, `Render`, and `Widgets`, since all three already need it
(`InputState::MousePosition` is `Platform`, `Renderer::DrawFilledRect` is
`Render`, `WidgetBase::Measure` is `Widgets`) and none of them should have to
depend on each other just to share a point/rect.

```cpp
namespace Penumbra {

struct Point {
    float X{0.0f};
    float Y{0.0f};
};

struct Rect {
    float X{0.0f};
    float Y{0.0f};
    float W{0.0f};
    float H{0.0f};
};

} // namespace Penumbra
```

### Required changes elsewhere

- `WidgetBase`: `Measure(Point) -> Point`, `Arrange(Rect)`,
  `GetArrangedRect() -> Rect`.
- `Box`: `MeasureContent(Point) -> Point`, `DrawContent(Renderer&, Rect)`,
  and its internal `FrameSize`/`ContentRectFrom` helpers, all retyped the
  same way — these are the exact methods Pharos overrides for the Inspector
  grid and Explorer rows, so this is the change that actually removes the
  SDL type from Pharos's own headers.
- `Renderer`: `DrawFilledRect`, `DrawRectOutline`, `DrawText`,
  `PushClipRect`, `DrawTexture` take `Rect`/`Point` instead of
  `SDL_FRect`/`SDL_FPoint`. Converts to `SDL_FRect`/`SDL_FPoint` internally at
  the point it calls SDL, same pattern as `Color` → `SDL_Color` in item 3.
- `InputState::MousePosition` becomes `Point`.
- `PlatformWindow::GetLogicalWindowSize()` returns `Point`.
- `ViewportWidget::OnRenderScene`'s `SceneSizeLogical` and
  `OnSceneInput`'s `ContentRect` become `Point`/`Rect`.

### What unblocks in Pharos

The Atlas panel's per-frame treemap drawing (`ViewportWidget::OnRenderScene`)
converts Pharos's own `layout::Rect` (already a distinct type — see
`src/layout/rect.h` — independent of whatever geometry type Penumbra uses)
into `Penumbra::Rect` at the call site, the same conversion it would do for
`SDL_FRect` today — but that conversion, and any custom `MeasureContent`/
`DrawContent` override in the Inspector/Explorer code, no longer needs
`#include <SDL3/SDL.h>` to spell the parameter types.

---

## 5. REQUIRED — Penumbra-owned input vocabulary (`Key`, `Modifiers`)

`InputState::KeysPressedThisFrame` is `std::vector<SDL_Keycode>` and
`InputState::ModifierState` is `SDL_Keymod`. Pharos's Atlas panel reads
`KeysPressedThisFrame` to detect Backspace/Escape for zoom-out (the direct
replacement for its old `ImGui::IsKeyPressed(ImGuiKey_Backspace)` /
`ImGuiKey_Escape` calls) — any code that compares against a specific key has
to name `SDLK_BACKSPACE` / `SDLK_ESCAPE`, another SDL header pulled into
Pharos application code for the same reason as items 3 and 4.

This one can stay deliberately small — Penumbra doesn't need to cover SDL's
full keycode space, only the keys its own widgets and documented use cases
already care about (`TextInput`'s scope: arrows, backspace, delete,
home/end, shift-select, Ctrl+A/C/V/X; plus Backspace/Escape for Pharos's
zoom-out). Anything not mapped comes through as `Key::Unknown` rather than
growing the enum to match SDL's.

### Proposed API

File: `include/Penumbra/Platform/InputState.h`

```cpp
namespace Penumbra::Platform {

enum class Key {
    Unknown,
    Left, Right, Up, Down,
    Home, End, Backspace, Delete, Escape, Enter, Tab,
    A, C, V, X, // the letters TextInput's Ctrl-shortcuts need; extend as widgets need more
};

struct Modifiers {
    bool Shift{false};
    bool Ctrl{false};
    bool Alt{false};
    bool Super{false};
};

struct InputState {
    Point MousePosition{};                    // depends on item 4
    bool  MouseButtonDown[3]{};
    bool  MouseButtonPressedThisFrame[3]{};
    bool  MouseButtonReleasedThisFrame[3]{};
    float MouseWheelDelta{0.0f};

    std::string       TextInputThisFrame;
    std::vector<Key>  KeysPressedThisFrame;
    Modifiers         ModifierState{};

    float DeltaTimeSeconds{0.0f};
};

} // namespace Penumbra::Platform
```

### Required changes elsewhere

- `PlatformWindow::PumpEventsAndBuildInput` maps each recognized
  `SDL_Keycode`/`SDL_Keymod` to `Key`/`Modifiers` once, internally, at the
  point it already builds `InputState` from SDL events — the same
  conversion-at-the-boundary pattern as items 3 and 4.
- `TextInput`'s existing key-handling switches from comparing `SDL_Keycode`
  values to comparing `Key` values. No behavior change, just the type its
  comparisons are written against.

### What unblocks in Pharos

The Atlas panel's zoom-out handling compares against `Key::Backspace` /
`Key::Escape` instead of `SDLK_BACKSPACE` / `SDLK_ESCAPE`, so it never needs
an SDL include either.

---

## 6. REQUIRED (minor) — two small `Platform` surface gaps

Two one-off additions, grouped together because both are small, both were
found by writing Pharos's actual bootstrap (`src/main.cpp`), and both exist
because the architecture's own rule — only `Penumbra::Platform` calls SDL —
leaves no legal workaround for either from outside Penumbra.

**a) `PlatformWindow::SetTitle`.** `Initialise` sets the window title once;
there's no way to change it afterward. Pharos wants to update the title to
the loaded project's name (`"Pharos — <projectName>"`), same as the old
`glfwSetWindowTitle` call it's replacing.

```cpp
// include/Penumbra/Platform/PlatformWindow.h
void SetTitle(const std::string& Title);
```

```cpp
// src/Penumbra/Platform/PlatformWindow.cpp
void PlatformWindow::SetTitle(const std::string& Title) {
    SDL_SetWindowTitle(Window, Title.c_str());
}
```

**b) A way to read the last platform error without calling `SDL_GetError()`.**
`PlatformWindow::Initialise` returns `bool` on failure with no detail; the
only way to find out *why* is to call `SDL_GetError()` directly, which is
what Pharos's current bootstrap does today (`src/main.cpp`'s
`fprintf(stderr, ..., SDL_GetError())`). Same shape as the other items here:
a diagnostic string, not a type, but still SDL reached for directly from
outside `Penumbra::Platform`.

```cpp
// include/Penumbra/Platform/PlatformWindow.h
std::string GetLastError() const;
```

```cpp
// src/Penumbra/Platform/PlatformWindow.cpp
std::string PlatformWindow::GetLastError() const {
    return SDL_GetError();
}
```

---

## 7. Explicitly not requested

- **Tabs / true docking (drag a panel into another region).** Pharos's four
  panels never share a region; nested `SplitPanel`s are sufficient. Not
  asking for this now — revisit only if a future Pharos layout actually needs
  it.
- **A tree-view or data-grid widget.** Buildable today from `Box` composition
  plus item 2 above. Keeping these in Pharos's own code, not Penumbra's
  catalogue, matches Penumbra's stated philosophy of composing the "div"
  rather than growing a widget-per-shape catalogue.
- **A popup/menu-overlay layer.** Pharos's File > Load menu becomes a
  persistently visible toolbar (`TextInput` + `Button`) instead of a
  dropdown. Functionally equivalent; not a framework gap.
  **Superseded 2026-07-22:** a real consumer needed one after all; see
  `Widgets::OverlayHost` (`include/Penumbra/Widgets/OverlayHost.h`). This
  bullet is left in place as the historical record of the original call —
  see `docs/next_steps.md` for current status.
- **A circular "radio dot" render primitive.** The Overlay selector's radio
  group reuses `Checkbox` styled as a filled square with app-level mutual
  exclusion. Aesthetic difference only.
- **Layout persistence (`imgui.ini` equivalent).** Nothing in Pharos depends
  on remembering split ratios across runs; can be added later as ordinary
  Pharos state (save `SplitRatio` values to a small local file) once item 1
  lands, without touching Penumbra.
- **`SDL_Texture*` in the public API (`ViewportWidget`, `Renderer::DrawTexture`).**
  Pharos's Atlas panel draws only primitives (rects, text) — no sprite/image
  loading — so it never has to spell `SDL_Texture` today. Already flagged as
  deferred in `viewport-widget-spec.md` ("Sprite / image drawing… out of
  scope here"); revisit alongside that work if Pharos ever needs textures,
  rather than solving it speculatively now.

---

## 8. What unblocks when this lands

Once items 1–5 exist, `src/ui/atlas_panel.cpp`, `explorer_panel.cpp`,
`inspector_panel.cpp`, and `overlay_selector.cpp` in Pharos get real Penumbra
implementations; `src/metrics/color_map.h`/`.cpp` and `src/main.cpp` drop
their direct SDL dependency entirely (no `#include <SDL3/SDL.h>`, no
`SDL_Color`/`SDL_FPoint`/`SDL_GetError` anywhere in Pharos's own source); and
`src/main.cpp` grows from the current bootstrap loop into building the real
widget tree (three `SplitPanel`s wrapping a `ViewportWidget` Atlas, a
`ScrollablePanel` Explorer tree, an Inspector grid, and an Overlay `Checkbox`
group) once per startup, per Penumbra's retained-mode model, rather than
redrawing panels from scratch every frame the way ImGui's immediate-mode
calls did. Items 6a/6b round out the window-lifecycle calls Pharos's
bootstrap already needs today.
