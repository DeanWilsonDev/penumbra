# Penumbra in Nox (prototype)

This branch is Penumbra's core rewritten in Nox, with no C++ left in it. It's written against the
Nox prototype compiler (`DeanWilsonDev/nox-systems-language`, branch `proto`), and its job is to
show that the prototype can carry a real retained-mode UI library on its own. The C++ library it
was ported from lives on the `proto` branch.

## Layout

- `src/` holds the library (`Geometry.nox`, `Maths.nox`, `Anim/`, `Platform/`, `Render/`,
  `Widgets/`) and the headless demo in `src/Demo/`.
- `nox.json` is the Nox project: every `.nox` file under `src/`, with `src/Demo/main.nox` as the
  entry file, built to `build/penumbra-demo`.
- `tests/check-demo.sh` runs the demo and diffs its output against `tests/demo-expected.txt`.
- `sdl/` is the SDL3 layer, also in Nox, and `sdl/Gallery/` a windowed gallery that uses it.
  `sdl/nox.json` is its own project: the library under `src/` without the headless demo, plus
  `sdl/`, built to `build/penumbra-gallery`. Keeping it out of `nox.json` keeps SDL out of the
  headless build.
- `tests/check-gallery-with-stub-sdl.sh` builds the gallery against `tests/sdl-stub/stub.c`, a small
  C stand-in for SDL3, SDL3_ttf and SDL3_image that checks every call's arguments (vertex colours
  and positions, index ranges, texture lifetimes, balanced render targets) and scripts 41 frames of
  input. It needs a Linux C compiler and no SDL, so it runs in a cloud session.

## Build and check

```sh
cmake -B build -G Ninja && cmake --build build   # in nox-systems-language, once
export NOX=../nox-systems-language/build/nox     # or put nox on PATH
$NOX build                                       # here: writes build/penumbra-demo
$NOX run                                         # builds and runs the demo
tests/check-demo.sh                              # runs it and diffs the output
```

`nox` finds `nox.json` in this directory or any directory above it, so these work from anywhere in
the checkout. Extra arguments to `tests/check-demo.sh` go to `nox run`, so
`tests/check-demo.sh --checked` builds with runtime provenance checks. `nox` compiles through a C
compiler (`$NOX_CC`, `$CC` or `cc`), which is a detail of the prototype's backend.

## The SDL window

The gallery opens a 960×680 window with buttons, a checkbox, a text field and the three charts,
drawn through `sdl/SdlRenderer.nox`. On macOS:

```sh
brew install sdl3 sdl3_ttf sdl3_image
cd sdl
LIBRARY_PATH=/opt/homebrew/lib nox run
```

`LIBRARY_PATH` tells the C compiler where Homebrew's libraries are; Linux finds them without it.
The gallery looks for its font in `sdl/assets/` relative to the directory it runs from, so run it
from `sdl/` or from the repository root.

## The demo

The demo builds a small window (a header with a flex spacer, a sidebar at 25%, a fixed-leading
content column, and a space-between footer) and measures and arranges it at 640×360. It prints
every widget's arranged rect, then draws through a recording renderer. Its output was checked
by hand against the C++ layout rules. It then runs eleven frames of simulated mouse input over a
toolbar holding a button and a disabled box, printing each frame's callbacks and interaction
states. Last it arranges a positioned panel with relative, absolute, fixed and sticky children.
Then it paints and probes a stage whose children use z-index, absolute, fixed and relative
positioning, a child clip rect and pointer-events none. Last it drives a `Button` whose background
eases between state colours, a rotated `Button` hit-tested through its transform, and a `Checkbox`
toggled by two clicks. Then it scrolls a `ScrollablePanel` of five rows by the wheel and by
dragging its scrollbar thumb. Last it focuses a `TextInput`, types UTF-8 text, moves and deletes
by code point, selects with shift and by dragging, and copies, pastes and cuts through a recording
clipboard. Last it routes keys: a focused `TextInput` keeps the keys it uses, the root's
`onKeyPressed` gets the rest, and a full-window modal takes every key while it's shown. Last it
drags a `SplitPanel`'s handle past its minimum pane size, with a vertical split nested inside.
Then it drags a `NumericDrag`, draws both `Chevron`s, and opens and dismisses overlays on an
`OverlayHost` by Escape and by a press outside. Last it opens a `Dropdown`, picks its long
item (drawn truncated in the trigger), and dismisses the menu by a backdrop press and by Escape.
Then it wraps six boxes into lines in an `InlineContainer`, skipping a hidden one and clamping one
wider than the line. Last it hovers, presses and disables an `IconWidget` drawn through a
recording icon backend. Last it draws a vertical `BarChart`, hovers a category, and draws a
horizontal stacked one with a negative bar and value labels along the bottom. Then a `LineChart`
whose area fill splits where the line crosses zero, and a `DonutChart` with a gap, an empty
slice, centre text and hover hit tests.
It also runs clean under ASan and UBSan.


## What's ported

| C++ (on `proto`) | Nox |
| --- | --- |
| `Geometry.h` | `src/Geometry.nox` (`Point`, `Rect`, `Transform`, plus `Numbers` helpers) |
| `Render/Color.h` | `src/Render/Color.nox` |
| `Platform/InputState.h` | `src/Platform/InputState.nox` (mouse, wheel, modifiers, occlusion, the `Key` enum, typed text, keys pressed this frame, and `KeyEvent`s for every press and release with their modifiers). `Keys:::Name` is the C++ `KeyName`: the web's `KeyboardEvent.key` name |
| `Render/Renderer.h`, `Render/IFontBackend.h` | `src/Render/Renderer.nox`, as the `IRenderer` and `IFontBackend` interfaces. Fonts are a size, not a handle, and `IFontBackend` has `MeasureText` and `MeasureAscent` (the C++ `TextMetrics.AscentLogical`) |
| `Widgets/Length.h` | `src/Widgets/Length.nox` (logical, percent and viewport units). `Resolve` takes the viewport, which comes from the `LayoutContext` |
| `Widgets/Styles.h` | `src/Widgets/Styles.nox` (`EdgeInsets`, the layout enums, the layout half of `BoxStyle`, its hovered, pressed and disabled colours, `borderRadius`, the two-stop gradient with hovered and pressed overrides, and the drop shadow). `IRenderer` has `FillRoundedRect`, `StrokeRoundedRect`, `FillGradientRect` and `DrawDropShadow`, and `RoundedRects:::Fill`/`Stroke` pick the square call when the radius is 0, as the C++ renderer does |
| `Widgets/WidgetBase.h` | `src/Widgets/WidgetBase.nox` (abstract class, with the seven callbacks as `(() -> void)!?` fields) |
| `Widgets/Box.h/.cpp` | `src/Widgets/Box.nox`: children as `Array<WidgetBase*!>!`, the None, vertical, horizontal and fixed-leading layouts, flex grow, cross alignment, justify, width, height, min-width and max-width, drawing the background, border and children in their interaction-state colours, and `UpdateInteractionState` (children in reverse order, occluded input after one consumes it, hover, press, release and disabled). `BoxStyle` transitions ease width, height, flex grow, background colour and opacity, each with its own optional `TransitionSpec` (duration and curve); `startingStyle` is where a box eases from when it's first laid out or shown again. A box at opacity 0 isn't drawn. A box with a transform or opacity below 1 paints as a group (`PaintsAsGroup`): it's layered by its stacking context and draws between `IRenderer.PushTransform` and `PopTransform`, and its input maps the pointer back through the inverse transform. A transformed box is the containing block for its fixed descendants too |
| `Widgets/LayoutPass.h/.cpp` | `LayoutContext.BeginPass`/`EndPass` and its `passId` (nested passes are ignored), with `Box`'s four-slot measure memo keyed by pass and available size |
| `Widgets/Positioning.h/.cpp` | `src/Widgets/Positioning.nox` and `LayoutContext.nox`: relative, absolute, fixed and sticky positioning, with the containing-block and scrollport stacks held by a `LayoutContext` passed to `Measure` and `Arrange` |
| `Widgets/Stacking.h/.cpp` | `src/Widgets/Stacking.nox`: z-index stacking contexts for paint and input, clip regions from `GetChildClipRect`, pointer-events and the pointer path. A `StackingTracker` passed to `Draw` and `UpdateInteractionState` holds what the C++ keeps in globals |
| `Widgets/Label.h/.cpp` | `src/Widgets/Label.nox`, measured through `IFontBackend`: word wrapping (cached per width, size and text), ellipsis truncation or clipping at `maxWidthLogical`, selection highlight, `IndexAtPoint` and `CaretRectAt`. It owns its text as a `string!`, set through `SetText` |
| `<cmath>` | `src/Maths.nox`: a `CMath` foreign block over libm (`expf`, `cosf`, `sinf`, `lroundf`) behind the safe `Maths` namespace |
| `Anim/Animation.h` | `src/Anim/Animation.nox`: `Approach`, `LerpColor`, the cubic and back easings, `Easing`, `AnimatedColor`, `Tween`, `TweenedValues` and the generic `EasedTrack<T>` (one eased property: from, target, timeline and curve) |
| `Widgets/Button.h/.cpp` | `src/Widgets/Button.nox`: click on release inside, an eased state background (Box's `BackgroundForState` is virtual for it), and hit testing through `BoxStyle.transform` |
| `Widgets/ScrollablePanel.h/.cpp` | `src/Widgets/ScrollablePanel.nox`: vertical and horizontal scrolling by wheel, the scrollbar track and thumb drag, the scrollport for sticky children, and the child clip. `Positioning:::OffsetInFlow` gained the C++'s optional sticky block |
| `Platform/IClipboard.h` | `src/Platform/Clipboard.nox`, the `IClipboard` interface; `GetClipboardText` returns an owned `string!` |
| `Render/TextWrap.h/.cpp` | `src/Render/TextWrap.nox`: `TextWrap:::Wrap` over an `ITextRangeMeasure` (the C++ range-measure callback, as an interface) and `TextWrap:::WithFont` |
| `Render/Utf8.h` | `src/Render/Utf8.nox`: `PreviousCodePoint` and `NextCodePoint` over `byteAt` |
| `Widgets/FocusState.h` | `src/Widgets/FocusState.nox` |
| `Widgets/TextSelection.h/.cpp` | `src/Widgets/TextSelection.nox`, enabled with `Box::EnableTextSelection` and run by `Box` and `ScrollablePanel`: drag selection across every visible `Label` under the box, Escape, select all and copy, with gaps filled between labels on one line and paragraphs joined by spaces. With no `dynamic_cast`, `WidgetBase` asks virtually: `AsSelectableText` (a `Label` answers itself as an `ISelectableText`), `StartsTextParagraph` (`InlineContainer`) and `TakesPointerFromTextSelection` (`TextArea`, `TextInput`, `Checkbox` and anything with press, release or change callbacks). The label walk is an explicit stack, and paragraphs are numbered rather than compared by pointer |
| `Widgets/TextArea.h/.cpp` | `src/Widgets/TextArea.nox`: wrapped multi-line editing with per-span font size, text colour and background (`TextAreaSpan`, with `fontSizeLogical` in place of a font handle and no class name), line boxes sized by each line's tallest run, vertical caret movement that keeps its column, Home and End per line, Enter, wheel scrolling, the caret kept in view and a scrollbar thumb. It implements `ITextRangeMeasure` so `TextWrap:::Wrap` measures through its spans. `ForEachRun`'s visitor became `RunsIn`, which returns the runs, and there's no font metrics cache |
| `Widgets/TextInput.h/.cpp` | `src/Widgets/TextInput.nox`: focus on click, typed text, caret and shift selection by code point, drag selection, backspace and delete, select all, copy, cut and paste, `onTextChanged` and `onFocusLost`. The text is an owned `string!` rebuilt from two substrings and the insertion on each edit |
| `Widgets/KeyDispatch.h/.cpp` | `src/Widgets/KeyDispatch.nox`: the outermost `Box` or `ScrollablePanel` opens a pass, a focused `TextInput` marks the keys it used, the first child that can act on the frame (`onKeyPressed` with presses, or `onKeyEvent` with presses or releases), or the root, gets the rest; presses without events reach `onKeyEvent` as down events; and a child covering the window withholds keys and text from everything after it. The `StackingTracker` owns the `KeyDispatch` the C++ keeps in a global, with the viewport it needs |
| `Widgets/SplitPanel.h/.cpp` | `src/Widgets/SplitPanel.nox`: horizontal and vertical splits, the ratio clamped by the minimum pane size, handle drag and hover colours, and the panes as optional owners (`WidgetBase*!?`). `SplitPanelStyle` isn't a struct of its own, since Nox structs don't inherit: its handle colours are fields on the panel |
| `Widgets/NumericDrag.h/.cpp` | `src/Widgets/NumericDrag.nox`, formatting its value with a template string (`${value:.2f}`) |
| `Widgets/Chevron.h/.cpp` | `src/Widgets/Chevron.nox`; `IRenderer` gains `DrawLine` |
| `Widgets/OverlayHost.h/.cpp` | `src/Widgets/OverlayHost.nox`: a root and a stack of overlays, Escape and outside-press dismissal, and dismissals deferred while the top overlay handles input. The overlays are parallel arrays (ids, owned contents, optional placements, dismiss flags), since a struct element can't own a widget |
| `Widgets/Dropdown.h/.cpp` | `src/Widgets/Dropdown.nox`: the trigger with its truncated label and chevron, a fixed backdrop and a menu that widens with a tween. The rows and the backdrop write the press to a shared `DropdownState` (the chosen index, a dismiss flag), and the dropdown applies it after dispatching to them, because a row's callback can't capture the dropdown |
| `Widgets/InlineContainer.h/.cpp` | `src/Widgets/InlineContainer.nox`: greedy line wrapping by width. The lines are one flat array of items that each carry their line index, since an array element can't own an array |
| `Widgets/CanvasWidget.h/.cpp`, `Widgets/ImageWidget.h/.cpp`, `Widgets/ViewportWidget.h/.cpp`, `Backends/IImageBackend.h` | `src/Widgets/CanvasWidget.nox` (an owned `onDraw` callable, clipped to the content rect), `src/Widgets/ImageWidget.nox` (loads through `IImageBackend`, releases through the backend it loaded with) and `src/Widgets/ViewportWidget.nox` (scene texture sized in physical pixels, recreated on resize, overlay children get first refusal, then `onSceneInput`). Textures are `u64` handles with 0 for none, and `IRenderer` gained `GetDpiScaleFactor`, `CreateTargetTexture`, `DestroyTexture`, `BeginTextureTarget`, `EndTextureTarget` and `DrawTexture` in place of the SDL calls; the viewport keeps the renderer that made its texture to destroy it |
| `Widgets/IconWidget.h/.cpp`, `Backends/IIconBackend.h` | `src/Widgets/IconWidget.nox`, and `IIconBackend` in `src/Render/Renderer.nox`: a fixed-size icon with per-state colours that fall back to the base colour when unset |
| `Widgets/Chart.h/.cpp`, `Widgets/BarChart.h/.cpp` | `src/Widgets/Chart.nox` (`ChartStyle`, `ChartSeries`, `ChartBase`, `AxisChartBase` with tick planning, grid, baseline and category labels) and `src/Widgets/BarChart.nox`. `ChartStyle` holds its `BoxStyle` as `box`, since structs don't inherit. `ForEachTick`'s visitor became a tick count and `TickAt`, and `DrawCategoryLabels`' centre callback a virtual `CategoryCentre`, since a lambda can't borrow `this`. Tick labels pick their decimals with a `when` over fixed-precision templates, as a template's precision is a literal. `IRenderer` gained `GetFrameIndex` and `GetFrameDeltaSeconds` for the appear and transition clock. Bars use `barCornerRadiusLogical` |
| `Widgets/LineChart.h/.cpp`, `Widgets/DonutChart.h/.cpp` | `src/Widgets/LineChart.nox` and `src/Widgets/DonutChart.nox`. The line chart keeps every series' points in one flat array and hands each series a `slice`, since an array element can't own an array. The donut's centre font is a size (`centreFontSizeLogical`), as the Nox font backend measures by size. `IRenderer` gained `DrawFilledCircle`, `DrawArc`, `DrawPolyline` (with `LineJoin` and `LineCap`) and `DrawFilledPolygon` |
| `Widgets/Checkbox.h/.cpp` | `src/Widgets/Checkbox.nox`, with `OnChanged(bool)` as `onCheckedChanged`, since `WidgetBase` already has an `onChanged` field |
| SDL3's headers | `sdl/SdlBindings.nox`: foreign blocks for SDL3, SDL3_ttf and SDL3_image, the C-layout structs they pass (`SdlVertex`, `SdlFRect`, the 128-byte `SdlEvent` and the event structs read out of it with `reinterpret`) and the constants Penumbra uses. Handles are `raw` pointers to opaque types |
| `Platform/PlatformWindow.h/.cpp` | `sdl/SdlWindow.nox`: `SdlWindow` (window, renderer, the event pump, mouse edges, modifiers with `shortcut` as super on macOS, typed text, key events, DPI and text input), `SdlClipboard` and `SdlKeys:::Map`. `FrameInput` lends the frame's typed text and key arrays into an `InputState` |
| `Render/Renderer.h/.cpp` | `sdl/SdlRenderer.nox` with `sdl/SdlGeometry.nox` and `sdl/SdlMesh.nox`: every `IRenderer` call, with the C++'s anti-aliased geometry (rounded rings, gradients, outlines, drop shadows, lines, arcs, polylines with joins and caps, ear-clipped polygons), the clip stack, transform groups through capture textures, and texture targets. Texture handles are slots in the renderer, shared with images. A transform group or texture target pushes an unbounded clip while it draws, where the C++ cleared the clip without pushing |
| `Render/SdlTtfFontBackend.h/.cpp` | `sdl/SdlFonts.nox`: `SdlFonts` implements `IFontBackend`, opening one `TTF_Font` per physical size, and `FontCache` caches rendered text textures, evicting ones unused for 120 frames |
| `Backends/SdlImageBackend.h/.cpp` | `sdl/SdlImages.nox`, loading through `IMG_LoadTexture` into the renderer's texture slots |

## Not ported yet

- The SDL layer hasn't been run against real SDL3 yet: the cloud sessions have no SDL3, so it's
  checked against the stub. The event and keycode constants in `sdl/SdlBindings.nox` were written
  from SDL 3.2's headers from memory and are the first thing to check if input misbehaves.

## Language friction found so far

The Nox board's Penumbra card tracks this list:

- **No file-level mutable state.** Penumbra keeps the layout viewport, the current layout pass and
  the containing-block stack in globals. The port passes a `LayoutContext` to `Measure` and
  `Arrange` instead, which holds the viewport, the running pass and the block and scrollport
  stacks.
- **No static members.** `Length::Logical(…)` became the namespace function
  `Lengths:::Logical(…)`.
- **`print` takes one value.** The recording renderer prints one value per line. Floats printed
  whole numbers in exponent form (`6.4e+02`) until nox #34 fixed it.
- **Text needed owned strings.** `TextInput` edits its text, so the prototype gained `string!`
  (concatenation, template strings, `copy`, nox #35) and `substring` and `byteAt` (nox #36). There's
  no in-place insert or erase: each edit builds a new string from two substrings and the insertion,
  deletes the old one and moves the new one in.
- **Pointer identity.** `focusState->focused == this` compares a `WidgetBase*` with a `TextInput*`;
  `==` across a class hierarchy arrived in nox #37.
- **No `\u{…}` escapes.** An unknown escape keeps the character after the backslash, as Nyx's lexer
  does, so the demo writes `é` literally.
- **No tuples.** `PlaceCross` returns a small `CrossPlacement` struct instead.
- **Callbacks can't borrow application state.** A callback is an owned closure,
  `(() -> void)!?`, and an owned closure may only copy or own what it captures (NOX0456). The
  C++ callbacks capture `this` or app objects by reference. Since nox #47 a callback can capture a
  `raw` pointer inside `unsafe`, which the gallery's button uses to update a label and a counter.
  Widgets that talk to their parts, like `Dropdown`'s rows, still write to a state object both
  sides borrow and the parent polls it.
- **SDL's C shapes.** Reading `SDL_Event` as its member structs needed `reinterpret<T>` (nox #46),
  SDL handles and C strings in structs needed raw pointers (nox #47), and vertex arrays, structs
  by value and `copyCString` came in nox #48. Flags and masks needed bitwise operators, which Nox
  didn't have (nox #49).
- **Interior mutability.** `IFontBackend.MeasureText` takes a shared receiver, but opening a font
  on first use changes the cache. `SdlFonts` reaches its `FontCache` through a `raw` pointer, as
  C++ would through a `mutable` member.
- **Disjoint fields counted as one borrow** until nox #80: `SDL_GetTextureSize(texture,
  rendered.width, rendered.height)` was NOX0461. Fixed; the font cache passes the fields directly.
- **`reinterpret` keeps a mutable borrow.** Binding `reinterpret<T>(event)` as a shared `T*`
  still records a mutable borrow of `event`, so reading `event.type` afterwards is NOX0459. The
  pump reads the event type first.
- **No program arguments.** `main` takes none, so the gallery tries a few font paths relative to
  where it runs.
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
  field defaults, optional owned callables, arrays of borrows and arrays of structs holding borrows,
  owned strings with substrings, pointer identity across a class hierarchy, and `~=` unwraps that
  borrow an optional owner mutably.
- **GCC 13.3 miscompiled the emitted C.** At `-O1`, it dropped the stores building a large struct
  temporary that was also zero-initialised at the top of `Box`'s constructor. Nox now leaves
  aggregate temporaries uninitialised (nox #42), since each is assigned before it's read.
