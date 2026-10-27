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
  states. It also runs clean under ASan and UBSan.

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
| `Widgets/Label.h/.cpp` | `Penumbra/Widgets/Label.nox`, measured through `IFontBackend` |

## Not ported yet

- The SDL platform layer, the SDL renderer and the SDL_ttf font backend. These need foreign
  calls into C, which the prototype doesn't have yet.
- Keyboard input, text input and key dispatch, pointer-events modes, and focus.
- Positioning, stacking contexts, clipping and the scroll containers.
- Transitions, animation, the layout-pass measure memo, and viewport units.
- Every widget beyond `Box` and `Label`.

## Language friction found so far

The Nox board's Penumbra card tracks this list:

- **No file-level mutable state.** Penumbra keeps the layout viewport and the current layout pass
  in globals, so viewport units and the measure memo are left out for now.
- **No static members.** `Length::Logical(…)` became the namespace function
  `Lengths:::Logical(…)`.
- **`print` takes one value, and strings don't concatenate yet.** The recording renderer prints one
  value per line, and floats print in their shortest form (`6.4e+02`).
- **No tuples.** `PlaceCross` returns a small `CrossPlacement` struct instead.
- **Callbacks can't borrow application state.** A callback is an owned closure,
  `(() -> void)!?`, and an owned closure may only copy or own what it captures (NOX0456). The
  C++ callbacks capture `this` or app objects by reference; in Nox the demo's button owns its
  counter. Wiring a callback to state the app also uses needs shared ownership or an unsafe
  escape, which the language hasn't designed yet.
- **No dereference copy.** `UpdateInteractionState` takes `InputState` by value, and
  `InputState.Occluded()` rebuilds the copy field by field.
- **Prototype features added for this port:** interfaces, arrays of owners
  (`Array<WidgetBase*!>!` with `freeAll()`), struct constructions and fixed array literals as
  field defaults, and optional owned callables.
