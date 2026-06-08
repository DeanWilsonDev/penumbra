# Penumbra — Proof-of-Concept Specification

> **Status:** PoC / proof of concept — throwaway-grade, expected to be rewritten
> **Scope of this document:** Penumbra ONLY. No Dawn. No `UmbraComponentLibrary`.
> **Target platform:** Linux primary, macOS secondary
> **Rendering backend:** SDL3 (`SDL_Renderer` path)
> **Text backend:** SDL_ttf (abstracted; FreeType swap later)
> **Language / build:** C++20 / CMake
> **Namespaces:** PascalCase — `Penumbra::`. No Hungarian prefixes (`F`, `E`, `U`).

This document supersedes `penumbra_design_document.md` wherever they disagree. That earlier
doc described an ImGui-first build, `F`-prefixed types, and a Penumbra-owned token system —
all of which have since been reversed.

---

## 1. What This PoC Is For

The single question this PoC must answer: **does the layered model hold up well enough that a
real tool can be built on it?**

Concretely, "success" for this first step is:

1. A vertical slice through every Penumbra layer — from the SDL3 window down to interactive widgets — actually connects and runs.
2. The slice ends in a small, runnable demo app that is built *using Penumbra's widgets*, the same way a future Dawn prototype would consume Penumbra.
3. The token/theme boundary is proven: the demo (not Penumbra) defines all values and the aesthetic; Penumbra only interprets them.

If that runs and feels reasonable to write tool code against, step one is a success. Everything
here is expected to change. The goal is to learn whether the shape is workable, not to ship.

### The Mental Model (shared vocabulary)

The architecture maps almost one-to-one onto the web stack. This mapping is the canonical way to
reason about responsibility:

| Web                         | Penumbra world                     |
| --------------------------- | ---------------------------------- |
| HTML elements (boxes)       | Penumbra primitive widgets (`Box` and its subclasses) |
| CSS properties on an element| **Tokens** — the styleable slots on a widget |
| CSS custom-property palette (`--spacing-s: 8px`) | **Theme** — app-defined named values |
| A component library (themed `<Button>`) | `UmbraComponentLibrary` (the middle tier — *not* built here) |
| A React app                 | Dawn (the tool — *not* built here) |

`Penumbra` is the library that provides the primitives and paints them. It has **no opinion** on
what anything should look like.

---

## 2. Architecture

A strict downward-only layer stack. Each layer calls only the layer beneath it.

```
┌──────────────────────────────────────────────────────────┐
│  Demo App  (throwaway sandbox — stands in for Dawn)        │  defines Theme + resolver, composes widgets
├──────────────────────────────────────────────────────────┤
│  Widget Library            (Penumbra::Widgets)             │  Box, Button, Label, Checkbox, NumericDrag,
│    widgets are STYLED BY values passed in — no defaults    │  TextInput, Panel, ScrollablePanel
├──────────────────────────────────────────────────────────┤
│  Renderer + Primitives     (Penumbra::Render)             │  DrawRect, DrawText, clip stack, DPI scale
│    wraps SDL_Renderer; owns the IFontBackend (SDL_ttf)     │  (SDL_GPU migration touches only this layer)
├──────────────────────────────────────────────────────────┤
│  Platform Backend          (Penumbra::Platform)           │  window, event loop → InputState, DPI query
│    the ONLY code that calls SDL directly                   │
└──────────────────────────────────────────────────────────┘
```

**Per-frame data flow:**

```
SDL events            → InputState snapshot (Platform)
Measure pass          → every widget reports desired size, bottom-up
Arrange pass          → every widget receives its final rect, top-down
UpdateInteractionState→ route InputState, update widget states, fire callbacks
Draw pass             → widgets emit primitives → Renderer → present
```

For the PoC the layout passes run **every frame**. Trees are tiny; correctness beats cleverness.
Dirty-flag invalidation is a later optimisation and does not change any public API.

---

## 3. Layer 1 — Platform Backend (`Penumbra::Platform`)

The only code in the project that calls SDL directly. Nothing above this layer includes an SDL
header for window/event purposes.

**Owns:**

- The single `SDL_Window`.
- The OS event loop: polls `SDL_Event` each frame and builds an `InputState` snapshot.
- DPI scale query (`SDL_GetWindowDisplayScale`), exposed as a single `float`.
- Text-input mode toggling (`SDL_StartTextInput` / `SDL_StopTextInput`) on behalf of the focused widget.

```cpp
namespace Penumbra::Platform {

struct InputState {
    SDL_FPoint MousePosition;                 // logical pixels
    bool       MouseButtonDown[3];            // left, middle, right — current
    bool       MouseButtonPressedThisFrame[3];
    bool       MouseButtonReleasedThisFrame[3];
    float      MouseWheelDelta;               // accumulated this frame

    std::string            TextInputThisFrame;   // from SDL text-input events
    std::vector<SDL_Keycode> KeysPressedThisFrame; // arrows, backspace, delete, etc.
    SDL_Keymod             ModifierState;
};

class PlatformWindow {
public:
    bool        Initialise(const char* Title, int LogicalWidth, int LogicalHeight);
    void        Shutdown();

    bool        PumpEventsAndBuildInput(InputState& OutInputState); // returns false on quit
    SDL_FPoint  GetLogicalWindowSize() const;
    float       GetDpiScaleFactor() const;

    void        SetTextInputActive(bool Active);
    SDL_Renderer* GetSdlRenderer() const;     // handed to the Render layer only
};

} // namespace Penumbra::Platform
```

> **Single primary window only.** Multi-window is a non-goal.

---

## 4. Layer 2 — Renderer + Primitives (`Penumbra::Render`)

Wraps `SDL_Renderer`. Exposes a small primitive API in **logical pixels**; the renderer applies
the DPI scale at submission. This is the layer — and the only layer — that an eventual `SDL_GPU`
port would replace.

```cpp
namespace Penumbra::Render {

using FontHandle = uint32_t;   // opaque handle into the font backend

struct TextMetrics {
    float WidthLogical;
    float HeightLogical;
    float AscentLogical;
};

class Renderer {
public:
    bool Initialise(SDL_Renderer* SdlRenderer, float DpiScaleFactor, IFontBackend* FontBackend);

    void BeginFrame(SDL_Color ClearColor);
    void EndFrameAndPresent();

    // All coordinates are LOGICAL pixels. The renderer multiplies by DPI scale internally.
    void DrawFilledRect (SDL_FRect RectLogical, SDL_Color Color);
    void DrawRectOutline(SDL_FRect RectLogical, SDL_Color Color, float ThicknessLogical);
    void DrawText       (FontHandle Font, std::string_view Text, SDL_FPoint PositionLogical, SDL_Color Color);

    // Clip stack — every push must be matched by a pop. Asserted balanced at EndFrameAndPresent.
    void PushClipRect(SDL_FRect RectLogical);
    void PopClipRect();

    // Measurement is in logical pixels even though glyphs are rasterised at physical size.
    TextMetrics MeasureText (FontHandle Font, std::string_view Text) const;
    float       MeasureTextWidth(FontHandle Font, std::string_view Text) const; // caret positioning

    float GetDpiScaleFactor() const;
};

} // namespace Penumbra::Render
```

### Rounded corners

`SDL_Renderer` has no native rounded-rect fill. `BorderRadius` stays in the token API so themes
can set it, but the **first cut renders square corners** (radius treated as a no-op). Rounding is
deferred to a later pass via `SDL_RenderGeometry` or a 9-slice texture. *Design for it, defer it.*

### Font backend abstraction

Text is the highest-leverage quality lever, but also the deepest rabbit hole. The PoC uses SDL_ttf
behind a clean interface so a FreeType + glyph-atlas upgrade later touches one file only.

```cpp
namespace Penumbra::Render {

class IFontBackend {
public:
    virtual ~IFontBackend() = default;

    // Loads a font at a given logical point size. The backend rasterises at size * DpiScale.
    virtual FontHandle  LoadFont(const char* Path, float PointSizeLogical, float DpiScaleFactor) = 0;

    virtual TextMetrics MeasureText     (FontHandle, std::string_view) const = 0;
    virtual float       MeasureTextWidth(FontHandle, std::string_view) const = 0;

    // Produces a texture for a run of text. Caching by (handle, string, color) is an
    // implementation detail of the backend, not required for the first cut.
    virtual SDL_Texture* AcquireTextTexture(SDL_Renderer*, FontHandle, std::string_view, SDL_Color) = 0;
};

class SdlTtfFontBackend : public IFontBackend { /* TTF_OpenFont / TTF_RenderText_Blended / TTF_GetStringSize */ };

} // namespace Penumbra::Render
```

### DPI rule (kept deliberately simple)

- Widgets and layout work entirely in **logical pixels**.
- The renderer multiplies geometry by `DpiScaleFactor` at submission.
- Fonts are loaded at `pointSize * DpiScaleFactor`, so glyph textures are physical-pixel sharp.
  `DrawText` scales the *position* but draws the glyph texture at its native physical size — no
  double scaling. `MeasureText` returns logical units (`physicalWidth / DpiScaleFactor`).

> **SDL3 API note:** exact SDL3 / SDL_ttf signatures (e.g. `TTF_GetStringSize`, clip-rect calls,
> `SDL_GetWindowDisplayScale`) should be confirmed against the installed headers at implementation
> time; SDL3 saw API churn during its stabilisation.

---

## 5. Layer 3 — Widget Library (`Penumbra::Widgets`)

### 5.1 The interface

```cpp
namespace Penumbra::Widgets {

class WidgetBase {
public:
    virtual ~WidgetBase() = default;

    // Two-phase layout.
    virtual SDL_FPoint Measure(SDL_FPoint AvailableSizeLogical) = 0; // returns desired size
    virtual void       Arrange(SDL_FRect FinalRectLogical) = 0;      // commits final rect

    // Returns true if this widget consumed the input (stops propagation).
    virtual bool       UpdateInteractionState(const Platform::InputState&) = 0;

    virtual void       Draw(Render::Renderer&) = 0;

    void     SetIsEnabled(bool Enabled);
    bool     GetIsEnabled() const;
    SDL_FRect GetArrangedRect() const;
};

} // namespace Penumbra::Widgets
```

### 5.2 `Box` — the universal primitive (the "div")

Everything is a box. A box owns the **box model**, an optional list of **children**, and a
**layout mode** for arranging those children. This is the HTML element model: there is no separate
container type — "container-ness" is just a box that has children.

```cpp
struct EdgeInsets { float Left, Top, Right, Bottom; };

enum class LayoutMode { None, VerticalStack, HorizontalStack };
enum class CrossAlign { Start, Center, End, Stretch };

// The universal style slots — the "tokens" every widget honours.
// Penumbra defines the SHAPE. It supplies NO values and NO semantic names.
struct BoxStyle {
    SDL_Color  ColorBackground;
    SDL_Color  ColorBorder;
    float      BorderWidth;
    float      BorderRadius;     // honoured by API; rendered square in first cut
    EdgeInsets Padding;          // inside the border  — the box's own job
    EdgeInsets Margin;           // outside the border — the PARENT's job during Arrange
};

class Box : public WidgetBase {
public:
    BoxStyle                                  Style;
    std::vector<std::unique_ptr<WidgetBase>>  Children;   // empty for leaves — costs ~one empty vector
    LayoutMode                                Layout    = LayoutMode::None;
    float                                     ChildGap  = 0.0f;
    CrossAlign                                CrossAlignment = CrossAlign::Start;

    WidgetBase* AddChild(std::unique_ptr<WidgetBase> Child);

    // Box draws background + border, insets the content rect by Padding, and arranges
    // Children according to Layout. Subclasses override DrawContent / MeasureContent for
    // their own intrinsic visuals (text, glyphs) and may override Arrange for custom layout.
protected:
    virtual SDL_FPoint MeasureContent(SDL_FPoint AvailableContentSize) { return {0, 0}; }
    virtual void       DrawContent(Render::Renderer&, SDL_FRect ContentRect) {}
};
```

**Margin vs padding ownership** (matches CSS, prevents layout drift):

- A box draws its own **background + border**, then insets by **padding** to find its content rect.
- A box's **margin** is consumed by its **parent** during `Arrange`: the parent subtracts each
  child's margin from the space it allocates before placing the child.

**Intrinsic content vs children** (the rule that collapses the widget catalogue):

> A widget draws only its own irreducible identity as *intrinsic content*. Anything you would vary
> is a *child*.

So a `Button` has no built-in label. Its visible content is its children:

- Text button → `Button` with one `Label` child.
- Icon button → `Button` with one icon child.
- Icon + text → `Button` with two children and `LayoutMode::HorizontalStack`.

No `TextButton` / `IconButton` / `IconTextButton` explosion. No child logic rebuilt anywhere.

**Footguns are accepted.** A `Label` may be given children it never lays out; they silently
vanish (HTML behaves the same with elements like `<br>`). We do **not** add type-system machinery
to prevent this — keeping the model simple is worth more than enforcing it.

### 5.3 Style inheritance

Per-widget styles extend `BoxStyle` so the box-model slots are universal and free:

```cpp
struct ButtonStyle : BoxStyle {
    SDL_Color ColorBackgroundHovered;
    SDL_Color ColorBackgroundPressed;
    SDL_Color ColorBackgroundDisabled;
    SDL_Color ColorLabel;            // applied to a Label child by the resolver, not by Button
};

struct CheckboxStyle : BoxStyle {
    SDL_Color ColorCheckMark;
    SDL_Color ColorBoxChecked;
};
// …one style struct per widget that needs extra slots.
```

### 5.4 Interaction model

Four states only — this is complete. If you think you need a fifth, you need a different widget.

```cpp
enum class InteractionState { Default, Hovered, Pressed, Disabled };
```

- **Click fires on release** (universal convention — lets the user cancel by dragging off).
- **Hit-test consumption:** the update pass walks the tree depth-first, children before parent, so
  the topmost widget under the cursor gets first refusal. The first widget to return `true` stops
  propagation. A `ScrollablePanel` restricts its children's hit region to its clipped bounds.
- **Keyboard focus** (needed by `TextInput` only, for the PoC): the demo app / root holds a single
  `WidgetBase* FocusedWidget`. A `TextInput` requests focus on click; text and key events route to
  the focused widget; the platform's text-input mode is toggled on focus change.

### 5.5 Widget catalogue (this PoC)

| Widget           | Is-a   | Adds                                                             |
| ---------------- | ------ | --------------------------------------------------------------- |
| `Box`            | —      | box model, children, layout                                     |
| `Label`          | `Box`  | intrinsic text content; leaf                                    |
| `Button`         | `Box`  | click + state-driven background fill                            |
| `Checkbox`       | `Box`  | boolean toggle; intrinsic check glyph                           |
| `NumericDrag`    | `Box`  | click-and-drag to change a float/int (Blender-style)            |
| `TextInput`      | `Box`  | single-line entry — see scope below                             |
| `Panel`          | `Box`  | essentially just a `Box` with a background + children           |
| `ScrollablePanel`| `Box`  | clip rect + scroll offset + wheel handling                      |

Stacks and "padding containers" are **not** separate types — they are a `Box` with a `LayoutMode`
and `Padding`. Convenience factories (e.g. `MakeVerticalPanel(...)`) provide ergonomics.

**`TextInput` scope for the PoC (deliberately minimal):** single line; visible caret;
type / backspace / delete; left / right arrow movement. **Deferred:** text selection, clipboard,
IME, multi-line. (`TextInput` is fundamental and Dawn needs it almost immediately; the deep parts
get a separate focused pass.)

---

## 6. Layer 4 — The Demo App (`Demo`)

The demo app is the throwaway sandbox that stands in for Dawn. It is the **only** place where
values and aesthetics exist. It does three things a real tool would do:

1. Defines a **Theme** — a flat palette of named values.
2. Provides **resolvers** that pour theme values into Penumbra style structs.
3. Composes Penumbra widgets into a window and wires up a few interactions.

```cpp
namespace Demo {

// The app owns the vocabulary AND the values. Penumbra never sees these names.
struct Theme {
    float SpacingSmall  = 8.0f;
    float SpacingMedium = 12.0f;
    float SpacingLarge  = 16.0f;

    SDL_Color ColorBackgroundPrimary   = {26, 26, 26, 255};
    SDL_Color ColorSurfaceRaised       = {58, 58, 65, 255};
    SDL_Color ColorAccent              = {77, 127, 235, 255};
    SDL_Color ColorTextPrimary         = {235, 235, 235, 255};
    SDL_Color ColorBorderDefault       = {70, 70, 75, 255};

    float BorderRadiusSmall = 3.0f;
    float BorderWidthDefault = 1.0f;
};

// Resolvers map semantic intent → concrete Penumbra style. This is what UmbraComponentLibrary
// will own later. Penumbra contains none of this.
Penumbra::Widgets::ButtonStyle ResolvePrimaryButtonStyle(const Theme&);
Penumbra::Widgets::BoxStyle    ResolvePanelStyle(const Theme&);

} // namespace Demo
```

The demo window should exercise the full slice — for example a small settings-style panel
containing: a heading `Label`; a `Button` that increments a counter shown in another `Label`; a
`Checkbox` that toggles a `Button`'s enabled state; a `NumericDrag` and a `TextInput` editing
values shown live; a `Separator`-style thin `Box`; all inside a `ScrollablePanel` to prove
clipping and scrolling.

**Hard rule:** no `SDL_Color` literal and no pixel literal appears anywhere under `Penumbra/`. If
you are tempted to write one there, it belongs in the demo's `Theme` instead. This rule is the
entire proof that Dawn can later own its own look.

---

## 7. Project Structure

```
penumbra/
├── CMakeLists.txt
├── README.md
├── vendor/                         # single-header deps only, if any
├── include/Penumbra/
│   ├── Platform/PlatformWindow.h
│   ├── Platform/InputState.h
│   ├── Render/Renderer.h
│   ├── Render/IFontBackend.h
│   ├── Render/SdlTtfFontBackend.h
│   └── Widgets/...                 # WidgetBase, Box, Button, Label, Checkbox,
│                                   #   NumericDrag, TextInput, Panel, ScrollablePanel, Styles
├── src/Penumbra/                   # matching .cpp files
└── demo/
    ├── main.cpp                    # event loop + composition root
    ├── DemoTheme.h / DemoTheme.cpp
    └── DemoResolvers.h / DemoResolvers.cpp
```

- `penumbra` builds as a **static library**. Nothing in it references `demo/`.
- `demo` is an **executable** that links `penumbra`. It is the only target that defines values.

---

## 8. Build

- **CMake** ≥ 3.24, **C++20**.
- Dependencies: `SDL3`, `SDL3_ttf`.

```bash
# via system packages / find_package
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/demo/penumbra_demo
```

`find_package(SDL3 CONFIG REQUIRED)` and `find_package(SDL3_ttf CONFIG REQUIRED)` are the expected
path. A `vcpkg install sdl3 sdl3-ttf` toolchain is the fallback if system packages are unavailable.
A font file (e.g. a mono TTF) is loaded at runtime — its path is a demo concern, not a Penumbra
concern. Font *selection* (Blex Mono / JetBrains Mono) is deferred.

---

## 9. Build Order & "Done When"

| # | Milestone            | Done when                                                                                   |
| - | -------------------- | ------------------------------------------------------------------------------------------- |
| 0 | Skeleton             | CMake builds; window opens and closes cleanly; screen clears to a color; DPI scale queried. |
| 1 | Renderer + font      | A filled rect, an outline, and a line of crisp text draw at correct DPI; clip push/pop works and is asserted balanced. |
| 2 | Box + style + layout | A `Box` draws background+border with padding; a parent box stacks two child boxes with gap + margins respected. |
| 3 | Input + first widget | `Button` changes background on hover/press, fires `OnClicked` on release, looks distinct when disabled. |
| 4 | Panels + scroll      | A `ScrollablePanel` clips and wheel-scrolls a column of widgets; window resize re-runs layout. |
| 5 | Remaining widgets    | `Label`, `Checkbox`, `NumericDrag`, and minimal `TextInput` each work interactively.        |
| 6 | Demo app             | The sandbox runs; all widgets compose and interact; every value/semantic lives only in the demo. |

Step one is a success when milestone 6 runs and writing the demo against Penumbra feels reasonable.

---

## 10. Out of Scope (this PoC)

- Dawn, and the `UmbraComponentLibrary` middle tier.
- Any token *values* or semantic names inside Penumbra.
- Animations (designed-for, not implemented).
- `SDL_GPU`; rounded-corner rendering; a FreeType glyph atlas.
- `TextInput` selection, clipboard, IME, multi-line.
- Multi-window, accessibility tree, declarative layout DSL, reactive data-binding.
- Dirty-flag layout invalidation (layout runs every frame for now).

---

## 11. Deferred Decisions (parked, not forgotten)

- Rounded-corner rendering strategy (`SDL_RenderGeometry` vs 9-slice).
- FreeType + glyph-atlas upgrade behind `IFontBackend`.
- Font selection (Blex Mono / JetBrains Mono).
- Layout generalisation (enum → strategy object) — note: the composition model is identical either
  way, so this never changes how widgets are built.
- Icon strategy (bundled icon font vs custom set).
