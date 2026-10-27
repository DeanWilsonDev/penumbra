# Penumbra in Nox (prototype)

This directory is a port of Penumbra's core to Nox. It's written against the Nox prototype
compiler (`DeanWilsonDev/nox-systems-language`, branch `proto`), and its job is to show that the
prototype can carry a real retained-mode UI library. The C++ library next to it is unchanged.

## Build and check

```sh
cmake -B build -G Ninja && cmake --build build      # in nox-systems-language
NOX=../nox-systems-language/build/nox nox/check.sh  # here
```

- `nox/build.sh` compiles the library and the headless demo into `build-nox/penumbra_nox_demo`. Any
  extra arguments go to `nox build`, so `nox/check.sh --checked` builds with runtime provenance
  checks.
- `nox/check.sh` runs the demo and diffs its output against `demo/expected.txt`.
- The demo builds a small window (a header with a flex spacer, a sidebar at 25%, a fixed-leading
  content column, and a space-between footer) and measures and arranges it at 640×360. It prints
  every widget's arranged rect, then draws through a recording renderer. Its output was checked
  by hand against the C++ layout rules. It then runs eleven frames of simulated mouse input over a
  toolbar holding a button and a disabled box, printing each frame's callbacks and interaction
  states. Last it arranges a positioned panel with relative, absolute, fixed and sticky children.
  Then it paints and probes a stage whose children use z-index, absolute, fixed and relative
  positioning, a child clip rect and pointer-events none. Last it drives a `Button` whose background
  eases between state colours, a rotated `Button` hit-tested through its transform, and a `Checkbox`
  toggled by two clicks. Then it scrolls a `ScrollablePanel` of five rows by the wheel and by
  dragging its scrollbar thumb.
  It also runs clean under ASan and UBSan.

## What's ported

| C++ | Nox |
| --- | --- |
| `Geometry.h` | `Penumbra/Geometry.nox` (`Point`, `Rect`, `Transform`, plus `Numbers` helpers) |
| `Render/Color.h` | `Penumbra/Render/Color.nox` |
| `Platform/InputState.h` | `Penumbra/Platform/InputState.nox` (mouse, wheel, modifiers, occlusion; no keys or text yet) |
| `Render/Renderer.h`, `Render/IFontBackend.h` | `Penumbra/Render/Renderer.nox`, as the `IRenderer` and `IFontBackend` interfaces |
| `Widgets/Length.h` | `Penumbra/Widgets/Length.nox` (logical and percent units) |
| `Widgets/Styles.h` | `Penumbra/Widgets/Styles.nox` (`EdgeInsets`, the layout enums, the layout half of `BoxStyle` and its hovered, pressed and disabled colours) |
| `Widgets/WidgetBase.h` | `Penumbra/Widgets/WidgetBase.nox` (abstract class, with the seven callbacks as `(() -> void)!?` fields) |
| `Widgets/Box.h/.cpp` | `Penumbra/Widgets/Box.nox`: children as `Array<WidgetBase*!>!`, the None, vertical, horizontal and fixed-leading layouts, flex grow, cross alignment, justify, width, height, min-width and max-width, drawing the background, border and children in their interaction-state colours, and `UpdateInteractionState` (children in reverse order, occluded input after one consumes it, hover, press, release and disabled) |
| `Widgets/Positioning.h/.cpp` | `Penumbra/Widgets/Positioning.nox` and `LayoutContext.nox`: relative, absolute, fixed and sticky positioning, with the containing-block and scrollport stacks held by a `LayoutContext` passed to `Arrange` |
| `Widgets/Stacking.h/.cpp` | `Penumbra/Widgets/Stacking.nox`: z-index stacking contexts for paint and input, clip regions from `GetChildClipRect`, pointer-events and the pointer path. A `StackingTracker` passed to `Draw` and `UpdateInteractionState` holds what the C++ keeps in globals |
| `Widgets/Label.h/.cpp` | `Penumbra/Widgets/Label.nox`, measured through `IFontBackend` |
| `<cmath>` | `Penumbra/Maths.nox`: a `CMath` foreign block over libm (`expf`, `cosf`, `sinf`, `lroundf`) behind the safe `Maths` namespace |
| `Anim/Animation.h` | `Penumbra/Anim/Animation.nox`: `Approach`, `LerpColor`, the cubic and back easings, `Easing`, `AnimatedColor`, `Tween` and `TweenedValues` |
| `Widgets/Button.h/.cpp` | `Penumbra/Widgets/Button.nox`: click on release inside, an eased state background (Box's `BackgroundForState` is virtual for it), and hit testing through `BoxStyle.transform` |
| `Widgets/ScrollablePanel.h/.cpp` | `Penumbra/Widgets/ScrollablePanel.nox`: vertical and horizontal scrolling by wheel, the scrollbar track and thumb drag, the scrollport for sticky children, and the child clip. `Positioning:::OffsetInFlow` gained the C++'s optional sticky block |
| `Widgets/Checkbox.h/.cpp` | `Penumbra/Widgets/Checkbox.nox`, with `OnChanged(bool)` as `onCheckedChanged`, since `WidgetBase` already has an `onChanged` field |

## Not ported yet

- The SDL platform layer, the SDL renderer and the SDL_ttf font backend. Foreign calls exist now,
  but only over bools, integers and floats: SDL needs opaque handles, raw pointers and C strings.
- Keyboard input, text input and key dispatch, pointer-events modes, and focus.
- Groups painted through a transform or opacity (`PaintsAsGroup`), and text selection inside a
  `ScrollablePanel`.
- `StackingContext::PointerTakerUsedWheel`: a layered descendant that took the wheel doesn't yet
  stop its scrolling ancestor from using it too.
- Transforms as containing blocks, and drawing through a transform: only `Button`'s hit test reads
  `BoxStyle.transform` so far.
- Rounded corners: `IRenderer` has no rounded fill, so `Checkbox` fills square rects.
- `BoxStyle`'s transitions, the layout-pass measure memo, and viewport units.
- Every widget beyond `Box`, `Label`, `Button`, `Checkbox` and `ScrollablePanel`.

## Language friction found so far

The Nox board's Penumbra card tracks this list:

- **No file-level mutable state.** Penumbra keeps the layout viewport, the current layout pass and
  the containing-block stack in globals. The port passes a `LayoutContext` to `Arrange` instead,
  which holds the viewport and the block and scrollport stacks. Viewport units and the measure
  memo would need it passed to `Measure` too, so they're still left out.
- **No static members.** `Length::Logical(…)` became the namespace function
  `Lengths:::Logical(…)`.
- **`print` takes one value, and strings don't concatenate yet.** The recording renderer prints one
  value per line. Floats printed whole numbers in exponent form (`6.4e+02`) until nox #34 fixed it.
- **No tuples.** `PlaceCross` returns a small `CrossPlacement` struct instead.
- **Callbacks can't borrow application state.** A callback is an owned closure,
  `(() -> void)!?`, and an owned closure may only copy or own what it captures (NOX0456). The
  C++ callbacks capture `this` or app objects by reference; in Nox the demo's button owns its
  counter. Wiring a callback to state the app also uses needs shared ownership or an unsafe
  escape, which the language hasn't designed yet.
- **Lists of borrowed widgets.** Stacking contexts keep the widgets they paint, so the prototype
  gained arrays of borrows (`Array<WidgetBase~*>!`) and arrays of structs that hold borrows. An
  entry's ancestors live in one pool array on the context, indexed by start and count, because a
  struct element can't own an array.
- **No dereference copy.** `UpdateInteractionState` takes `InputState` by value, and
  `InputState.Occluded()` rebuilds the copy field by field.
- **No maths library.** Penumbra declares its own libm foreign block; every call sits in an
  `unsafe` block inside a small safe function.
- **Prototype features added for this port:** foreign C calls inside `unsafe` blocks, interfaces, arrays of owners
  (`Array<WidgetBase*!>!` with `freeAll()`), struct constructions and fixed array literals as
  field defaults, optional owned callables, arrays of borrows and arrays of structs holding borrows.
