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

| C++ | Nox |
| --- | --- |
| `Geometry.h` | `Penumbra/Geometry.nox` (`Point`, `Rect`, `Transform`, plus `Numbers` helpers) |
| `Render/Color.h` | `Penumbra/Render/Color.nox` |
| `Platform/InputState.h` | `Penumbra/Platform/InputState.nox` (mouse, wheel, modifiers, occlusion, the `Key` enum, typed text and keys pressed this frame) |
| `Render/Renderer.h`, `Render/IFontBackend.h` | `Penumbra/Render/Renderer.nox`, as the `IRenderer` and `IFontBackend` interfaces. Fonts are a size, not a handle, and `IFontBackend` has `MeasureText` and `MeasureAscent` (the C++ `TextMetrics.AscentLogical`) |
| `Widgets/Length.h` | `Penumbra/Widgets/Length.nox` (logical and percent units) |
| `Widgets/Styles.h` | `Penumbra/Widgets/Styles.nox` (`EdgeInsets`, the layout enums, the layout half of `BoxStyle`, its hovered, pressed and disabled colours, `borderRadius`, the two-stop gradient with hovered and pressed overrides, and the drop shadow). `IRenderer` has `FillRoundedRect`, `StrokeRoundedRect`, `FillGradientRect` and `DrawDropShadow`, and `RoundedRects:::Fill`/`Stroke` pick the square call when the radius is 0, as the C++ renderer does |
| `Widgets/WidgetBase.h` | `Penumbra/Widgets/WidgetBase.nox` (abstract class, with the seven callbacks as `(() -> void)!?` fields) |
| `Widgets/Box.h/.cpp` | `Penumbra/Widgets/Box.nox`: children as `Array<WidgetBase*!>!`, the None, vertical, horizontal and fixed-leading layouts, flex grow, cross alignment, justify, width, height, min-width and max-width, drawing the background, border and children in their interaction-state colours, and `UpdateInteractionState` (children in reverse order, occluded input after one consumes it, hover, press, release and disabled) |
| `Widgets/Positioning.h/.cpp` | `Penumbra/Widgets/Positioning.nox` and `LayoutContext.nox`: relative, absolute, fixed and sticky positioning, with the containing-block and scrollport stacks held by a `LayoutContext` passed to `Arrange` |
| `Widgets/Stacking.h/.cpp` | `Penumbra/Widgets/Stacking.nox`: z-index stacking contexts for paint and input, clip regions from `GetChildClipRect`, pointer-events and the pointer path. A `StackingTracker` passed to `Draw` and `UpdateInteractionState` holds what the C++ keeps in globals |
| `Widgets/Label.h/.cpp` | `Penumbra/Widgets/Label.nox`, measured through `IFontBackend`: word wrapping (cached per width, size and text), ellipsis truncation or clipping at `maxWidthLogical`, selection highlight, `IndexAtPoint` and `CaretRectAt`. It owns its text as a `string!`, set through `SetText` |
| `<cmath>` | `Penumbra/Maths.nox`: a `CMath` foreign block over libm (`expf`, `cosf`, `sinf`, `lroundf`) behind the safe `Maths` namespace |
| `Anim/Animation.h` | `Penumbra/Anim/Animation.nox`: `Approach`, `LerpColor`, the cubic and back easings, `Easing`, `AnimatedColor`, `Tween` and `TweenedValues` |
| `Widgets/Button.h/.cpp` | `Penumbra/Widgets/Button.nox`: click on release inside, an eased state background (Box's `BackgroundForState` is virtual for it), and hit testing through `BoxStyle.transform` |
| `Widgets/ScrollablePanel.h/.cpp` | `Penumbra/Widgets/ScrollablePanel.nox`: vertical and horizontal scrolling by wheel, the scrollbar track and thumb drag, the scrollport for sticky children, and the child clip. `Positioning:::OffsetInFlow` gained the C++'s optional sticky block |
| `Platform/IClipboard.h` | `Penumbra/Platform/Clipboard.nox`, the `IClipboard` interface; `GetClipboardText` returns an owned `string!` |
| `Render/TextWrap.h/.cpp` | `Penumbra/Render/TextWrap.nox`: `TextWrap:::Wrap` over an `ITextRangeMeasure` (the C++ range-measure callback, as an interface) and `TextWrap:::WithFont` |
| `Render/Utf8.h` | `Penumbra/Render/Utf8.nox`: `PreviousCodePoint` and `NextCodePoint` over `byteAt` |
| `Widgets/FocusState.h` | `Penumbra/Widgets/FocusState.nox` |
| `Widgets/TextSelection.h/.cpp` | `Penumbra/Widgets/TextSelection.nox`, enabled with `Box::EnableTextSelection` and run by `Box` and `ScrollablePanel`: drag selection across every visible `Label` under the box, Escape, select all and copy, with gaps filled between labels on one line and paragraphs joined by spaces. With no `dynamic_cast`, `WidgetBase` asks virtually: `AsSelectableText` (a `Label` answers itself as an `ISelectableText`), `StartsTextParagraph` (`InlineContainer`) and `TakesPointerFromTextSelection` (`TextArea`, `TextInput`, `Checkbox` and anything with press, release or change callbacks). The label walk is an explicit stack, and paragraphs are numbered rather than compared by pointer |
| `Widgets/TextArea.h/.cpp` | `Penumbra/Widgets/TextArea.nox`: wrapped multi-line editing with per-span font size, text colour and background (`TextAreaSpan`, with `fontSizeLogical` in place of a font handle and no class name), line boxes sized by each line's tallest run, vertical caret movement that keeps its column, Home and End per line, Enter, wheel scrolling, the caret kept in view and a scrollbar thumb. It implements `ITextRangeMeasure` so `TextWrap:::Wrap` measures through its spans. `ForEachRun`'s visitor became `RunsIn`, which returns the runs, and there's no font metrics cache |
| `Widgets/TextInput.h/.cpp` | `Penumbra/Widgets/TextInput.nox`: focus on click, typed text, caret and shift selection by code point, drag selection, backspace and delete, select all, copy, cut and paste, `onTextChanged` and `onFocusLost`. The text is an owned `string!` rebuilt from two substrings and the insertion on each edit |
| `Widgets/KeyDispatch.h/.cpp` | `Penumbra/Widgets/KeyDispatch.nox`: the outermost `Box` or `ScrollablePanel` opens a pass, a focused `TextInput` marks the keys it used, the first child with `onKeyPressed` that saw keys (or the root) gets the rest, and a child covering the window withholds keys and text from everything after it. The `StackingTracker` owns the `KeyDispatch` the C++ keeps in a global, with the viewport it needs |
| `Widgets/SplitPanel.h/.cpp` | `Penumbra/Widgets/SplitPanel.nox`: horizontal and vertical splits, the ratio clamped by the minimum pane size, handle drag and hover colours, and the panes as optional owners (`WidgetBase*!?`). `SplitPanelStyle` isn't a struct of its own, since Nox structs don't inherit: its handle colours are fields on the panel |
| `Widgets/NumericDrag.h/.cpp` | `Penumbra/Widgets/NumericDrag.nox`, formatting its value with a template string (`${value:.2f}`) |
| `Widgets/Chevron.h/.cpp` | `Penumbra/Widgets/Chevron.nox`; `IRenderer` gains `DrawLine` |
| `Widgets/OverlayHost.h/.cpp` | `Penumbra/Widgets/OverlayHost.nox`: a root and a stack of overlays, Escape and outside-press dismissal, and dismissals deferred while the top overlay handles input. The overlays are parallel arrays (ids, owned contents, optional placements, dismiss flags), since a struct element can't own a widget |
| `Widgets/Dropdown.h/.cpp` | `Penumbra/Widgets/Dropdown.nox`: the trigger with its truncated label and chevron, a fixed backdrop and a menu that widens with a tween. The rows and the backdrop write the press to a shared `DropdownState` (the chosen index, a dismiss flag), and the dropdown applies it after dispatching to them, because a row's callback can't capture the dropdown |
| `Widgets/InlineContainer.h/.cpp` | `Penumbra/Widgets/InlineContainer.nox`: greedy line wrapping by width. The lines are one flat array of items that each carry their line index, since an array element can't own an array |
| `Widgets/CanvasWidget.h/.cpp`, `Widgets/ImageWidget.h/.cpp`, `Widgets/ViewportWidget.h/.cpp`, `Backends/IImageBackend.h` | `Penumbra/Widgets/CanvasWidget.nox` (an owned `onDraw` callable, clipped to the content rect), `Penumbra/Widgets/ImageWidget.nox` (loads through `IImageBackend`, releases through the backend it loaded with) and `Penumbra/Widgets/ViewportWidget.nox` (scene texture sized in physical pixels, recreated on resize, overlay children get first refusal, then `onSceneInput`). Textures are `u64` handles with 0 for none, and `IRenderer` gained `GetDpiScaleFactor`, `CreateTargetTexture`, `DestroyTexture`, `BeginTextureTarget`, `EndTextureTarget` and `DrawTexture` in place of the SDL calls; the viewport keeps the renderer that made its texture to destroy it |
| `Widgets/IconWidget.h/.cpp`, `Backends/IIconBackend.h` | `Penumbra/Widgets/IconWidget.nox`, and `IIconBackend` in `Penumbra/Render/Renderer.nox`: a fixed-size icon with per-state colours that fall back to the base colour when unset |
| `Widgets/Chart.h/.cpp`, `Widgets/BarChart.h/.cpp` | `Penumbra/Widgets/Chart.nox` (`ChartStyle`, `ChartSeries`, `ChartBase`, `AxisChartBase` with tick planning, grid, baseline and category labels) and `Penumbra/Widgets/BarChart.nox`. `ChartStyle` holds its `BoxStyle` as `box`, since structs don't inherit. `ForEachTick`'s visitor became a tick count and `TickAt`, and `DrawCategoryLabels`' centre callback a virtual `CategoryCentre`, since a lambda can't borrow `this`. Tick labels pick their decimals with a `when` over fixed-precision templates, as a template's precision is a literal. `IRenderer` gained `GetFrameIndex` and `GetFrameDeltaSeconds` for the appear and transition clock. Bars use `barCornerRadiusLogical` |
| `Widgets/LineChart.h/.cpp`, `Widgets/DonutChart.h/.cpp` | `Penumbra/Widgets/LineChart.nox` and `Penumbra/Widgets/DonutChart.nox`. The line chart keeps every series' points in one flat array and hands each series a `slice`, since an array element can't own an array. The donut's centre font is a size (`centreFontSizeLogical`), as the Nox font backend measures by size. `IRenderer` gained `DrawFilledCircle`, `DrawArc`, `DrawPolyline` (with `LineJoin` and `LineCap`) and `DrawFilledPolygon` |
| `Widgets/Checkbox.h/.cpp` | `Penumbra/Widgets/Checkbox.nox`, with `OnChanged(bool)` as `onCheckedChanged`, since `WidgetBase` already has an `onChanged` field |

## Not ported yet

- The SDL platform layer, the SDL renderer and the SDL_ttf font backend. Foreign calls now carry
  opaque handles, C strings and out-parameters of C-layout structs (nox #38, #39); SDL still needs
  callbacks and a way to read its `SDL_Event` union as one of its member structs.
- `KeyEvent`, `OnKeyEvent` and `KeyName`: keys reach handlers only as `onKeyPressed`.
- Groups painted through a transform or opacity (`PaintsAsGroup`).
- `StackingContext::PointerTakerUsedWheel`: a layered descendant that took the wheel doesn't yet
  stop its scrolling ancestor from using it too.
- Transforms as containing blocks, and drawing through a transform: only `Button`'s hit test reads
  `BoxStyle.transform` so far.
- `BoxStyle`'s transitions, the layout-pass measure memo, and viewport units.

## Language friction found so far

The Nox board's Penumbra card tracks this list:

- **No file-level mutable state.** Penumbra keeps the layout viewport, the current layout pass and
  the containing-block stack in globals. The port passes a `LayoutContext` to `Arrange` instead,
  which holds the viewport and the block and scrollport stacks. Viewport units and the measure
  memo would need it passed to `Measure` too, so they're still left out.
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
  C++ callbacks capture `this` or app objects by reference; in Nox the demo's button owns its
  counter. Wiring a callback to state the app also uses needs shared ownership or an unsafe
  escape, which the language hasn't designed yet. Widgets that talk to their parts, like
  `Dropdown`'s rows, write to a state object both sides borrow and the parent polls it.
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
