# Feature request: style-surface gaps found scoping Lustre

> **Status:** Request, not yet scoped or implemented. Filed during Lustre's design handoff
> (`../../lustre/docs/lustre_handoff.md` §3) — not blocking that design; Lustre's spec includes
> these properties/selectors now and stubs them at the application boundary until Penumbra
> grows the equivalent support, per that doc's growing-together principle.

## 1. Interaction-state (hover/pressed/disabled) styling is Button-only today

`Widgets/Styles.h`'s `BoxStyle` (used directly by `Frame`, and as the base every other style
struct extends) has no interaction-state fields at all — no hover/pressed/disabled colors,
borders, or anything else. Only `ButtonStyle` adds
`ColorBackgroundHovered`/`ColorBackgroundPressed`/`ColorBackgroundDisabled`; `CheckboxStyle`
adds none. Lustre's `:hover`/`:active`/`:disabled` pseudo-class selectors are being designed
to apply to *any* classed element, matching how Iris's own interactivity model already treats
event props (`onPress` etc.) as valid on any element, not just `Button` — see
`iris/docs/iris_core_spec.md` §4's "a `Frame` with an `onPress` prop is an interactive frame."
Lustre resolving a `:hover` rule against a `Frame`- or `Checkbox`-classed element currently has
nowhere to put the result.

### What would unblock this

Interaction-state color fields (hover/pressed/disabled, matching `ButtonStyle`'s existing
three) moved down onto `BoxStyle` itself, so every widget type gets them for free, the same
way `Padding`/`Margin`/`BorderRadius` already are universal box-model slots rather than
per-widget additions. `Button`'s own interaction-state detection (`UpdateInteractionState`,
already tracking hover/press/disabled from `InputState` per frame) wouldn't need to change —
only widgets that want to *use* the new fields (starting with `Frame`, given it's the
`onPress`-capable general-purpose container) would need their own
`Draw`/interaction-detection logic extended to read them, mirroring `Button`'s existing
pattern.

## 2. No transform primitive of any kind

No scale, translate, or rotate exists anywhere in `Renderer`, `WidgetBase`, or any style
struct. Lustre's spec includes `transform: scale(...)` as a real property (documented, not
omitted) because it's a common enough styling need (press-state feedback, hover lift) that
leaving it out of the language entirely felt wrong even though nothing can execute it today —
see the old Lustre design draft's own `:active { transform: scale(0.97) }` example, which
doesn't compile to anything real right now.

### What would unblock this

No proposed API here — this is a bigger primitive than the others (affects layout/hit-testing,
not just drawing) and deserves its own design pass when it's actually prioritized, not a
speculative signature bolted on as a side effect of Lustre's own design. Recording the need,
not the shape.

## 3. No fixed-size (width/height) override anywhere

`WidgetBase`/`Box`/`Label` size themselves entirely intrinsically — `Measure`/`Arrange` derive
size from content plus `BoxStyle::Padding`, with no field anywhere to say "be exactly 200
logical pixels wide regardless of content." Lustre's spec includes `width`/`height` as real
properties (two of the most basic in CSS, and Lustre's explicit design goal is CSS
familiarity) even though nothing backs them today.

### What would unblock this

An explicit-size-override concept distinct from intrinsic `Measure` — e.g. optional
`WidthLogical`/`HeightLogical` fields (or a `SizeMode` enum: `Intrinsic` vs `Fixed`) that
`Measure` consults before falling through to content-driven sizing. Not proposing exact field
names here; this needs its own design pass alongside whatever `<Grid>`'s real layout mode ends
up needing (`iris_core_spec.md` §9's own open gap), since both touch `Measure`/`Arrange`.

## 4. Not a gap, confirmed working as-is

Penumbra's `Button::UpdateInteractionState` already does its own hover/press/disabled
detection per frame from `Platform::InputState` — Lustre/Iris will never need to detect
interaction state themselves for any widget, only supply a complete resolved style struct once
per class change. Recorded here so it's clear this *isn't* part of the gap — worth confirming
the same holds if/when item 1 above extends interaction-state fields to `Frame`/`Checkbox`.
