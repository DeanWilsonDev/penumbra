# Iris — Handoff: Scoping Iris Core Against Penumbra as a First Backend

> **Status:** Design handoff — no Iris code exists yet.
> **Scope:** How Iris's language/runtime should be split so it can target both Penumbra
> (2D desktop tool UI, available now) and Umbra Engine/Nyx (in-game UI, later) without the
> two diverging into separate DSLs.

## 1. Why this document exists

Iris was originally designed against Umbra Engine + Nyx as its only backend — Nyx decides
which component is active, Nyx drives 3D animation state, Umbra's state machine owns
routing. But Umbra Engine doesn't exist yet, and Penumbra — a small, real, working C++20/SDL3
retained-mode widget library — already does, with two real consumers: **Pharos** (an existing
external tool) and **Dawn** (Umbra's 2D level editor, upcoming). Building Iris against
Penumbra first proves the reactive-runtime and styling machinery against a real target years
before Umbra Engine can host it, and gives Pharos/Dawn a shared authoring experience with the
in-game UI that will eventually use the same language.

## 2. The core decision: one Iris Core, capability-gated backend extensions

Not two DSLs, and not silent unavailability. Concretely:

- **Iris Core** — backend-agnostic — is everything already in the original design that
  doesn't depend on a specific engine: primitives (`frame`, `inline`, `grid`), the
  component model (props, state, events, composition, conditionals/loops in `render`),
  imports, and Lustre-lite styling.
- **Backend capabilities** — features that only one backend can support are tagged, and
  referencing a capability the compile target doesn't have is a **compile-time error**, not
  a runtime no-op. Example: `<model3d>` requires the `umbra-engine` backend; compiling an
  Iris file that uses it with `--target penumbra` fails at compile time with a clear message.
- This is a well-trodden pattern, not a novel risk: React Native vs React DOM host
  components, Kotlin Multiplatform `expect`/`actual`, SwiftUI `#if os()`.
- Two backends to design for: `penumbra` (now) and `umbra-engine` (later, deferred — see
  §6 Stage 6). Nothing in Stages 0–5 below should depend on `umbra-engine` existing.

## 3. Why Penumbra first

- It's real and working today, not speculative.
- Two consumers are already lined up: Pharos (existing), Dawn (upcoming, Umbra's own level
  editor — eating Iris's own dog food inside the Umbra ecosystem before the game-facing
  backend exists).
- Penumbra's own spec already anticipated a themed middle tier it calls
  `UmbraComponentLibrary`, sitting between raw Penumbra widgets and a real consuming tool —
  Iris-on-Penumbra is a natural fit for that reserved slot.
- Penumbra's styling work (gradients, radial gradients, drop shadows, blend modes —
  see `Render::Renderer`'s `DrawGradientRect`/`DrawRadialGradient`/`DrawDropShadow`/
  `PushBlendMode`) already gives Lustre-lite a real, modern token surface to target,
  not just flat colors.

## 4. What Iris needs to build regardless of backend

These are hard parts of Iris itself — no backend choice avoids them:

- **Reactive state + reconciliation.** `state { ... }` and conditional rendering
  (`if (settingsOpen) { <SettingsPage/> }`) imply diffing a new render against the previous
  one and applying minimal mutations — a real reconciler, not just a tree builder.
- **Stable element identity (`key`).** Not present in the original Iris design doc. A
  reconciler needs a way to match new elements to existing instances across re-renders
  (lists, conditionals) — this needs to be added to Iris Core's prop model at the language
  design stage (Stage 0), not bolted on later.
- **Lustre-lite style resolution.** Global + component-scoped precedence, class selectors,
  resolving down to concrete values per backend. This doc doesn't attempt to design Lustre
  itself — flagged as a dependency, not solved here.
- **Module/import resolution.** `import HealthBar` → file search path → component graph.
  Backend-independent; needed before any backend can compile anything.

## 5. What the Penumbra backend specifically needs

- A codegen or runtime interpreter that turns a rendered Iris component tree into calls
  against Penumbra's existing `Box`/`AddChild`/`ApplyStyle`/field-mutation API
  (`include/Penumbra/Widgets/Box.h`, `Styles.h`) — this mapping is direct and already proven
  by hand in `demo/main.cpp`.
- Because re-renders must produce mutations, not full rebuilds, the Penumbra backend runtime
  needs to retain a map from Iris element identity → live `WidgetBase*`, matching Penumbra's
  own model of a persistent, mutate-in-place tree that re-runs layout every frame.
- Event props (`onPress`, `onChange`) map directly onto Penumbra's existing
  `std::function` callback members (`Button::OnClicked`, `Checkbox::OnChanged`) — no new
  binding mechanism needed on Penumbra's side; this part already lines up cleanly.
- **Resolved:** Penumbra's child-mutation API was append-only when this handoff was written;
  it now has `Box::RemoveChild/ReplaceChild/ClearChildren/MoveChild/InsertChildAt`
  (`include/Penumbra/Widgets/Box.h`) plus the generic `WidgetBase::GetChildCount/GetChildAt`
  enumeration every widget implements, so the reconciler's diff/minimal-mutation step (Stage 3)
  is unblocked whenever Iris gets there.

## 6. Proposed phased roadmap

| Stage | Scope |
| --- | --- |
| 0 | Formalize the Iris Core language spec: grammar, component/props/state/event model, add the `key` prop, define the backend-capability tagging mechanism. Pure language design, no code. |
| 1 | Front end: lexer/parser/AST for Iris Core (structure + logic only — Lustre and full reactivity can come later). |
| 2 | Penumbra backend, static slice first: build a Penumbra `Box` tree once from a parsed, props-resolved Iris component tree. No state, no re-render yet — proves the structural mapping end to end. |
| 3 | Reactive runtime: state, re-render triggers, the reconciler (diff + minimal mutation), consuming Penumbra's child-mutation API once the companion feature request lands. |
| 4 | Lustre-lite: global + component-scoped style resolution mapped onto Penumbra's `BoxStyle`/`ButtonStyle`/etc. and its gradient/shadow/blend-mode primitives. |
| 5 | First real consumer: port a real slice of Pharos (or a new Dawn panel) to Iris — validates the pipeline against real UI, not a toy demo. |
| 6 (deferred) | Umbra Engine/Nyx backend, `model3d`, engine-driven routing — gated behind the Stage 0 capability system from day one so nothing in Stages 0–5 accidentally depends on it. |

## 7. Open questions for Stage 0

- Exact syntax for backend capability tags — per-element attribute, per-import, or a
  file-level pragma?
- Is `key` a reserved prop name or its own syntax construct?
- Lustre's cascade/precedence rules aren't designed yet — Stage 4 depends on that design
  existing; out of scope for this document.
- Where the new Iris project/repo will live, and whether it depends on Penumbra as a
  submodule/package or vendors it.
