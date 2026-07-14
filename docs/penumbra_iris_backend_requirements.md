# Penumbra — Backend-Readiness Requirements for Iris

> **Scope:** Gaps found while scoping Penumbra as a rendering backend for Iris (a reactive
> UI compiler/runtime planned as a separate project — see `docs/iris_handoff.md`). Purely
> about whether Penumbra's widget-tree API is sufficient for a reconciler to drive it; no
> visual, layout, or styling changes proposed here.
> **Status:** Items 1–3 (child removal/replacement/clear, reordering, uniform child
> enumeration) are implemented — see `Box::RemoveChild`/`ReplaceChild`/`ClearChildren`/
> `MoveChild`/`InsertChildAt` and `WidgetBase::GetChildCount`/`GetChildAt` (overridden by
> `Box` and `SplitPanel`). Landed ahead of Iris's reactive runtime existing, since the API
> is generically useful (see §6) independent of Iris's own timeline.

---

## 1. REQUIRED — child removal, replacement, and clear

`Box::AddChild` (`include/Penumbra/Widgets/Box.h`) is the only named mutation entry point —
append-only. `Children` is a public `std::vector<std::unique_ptr<WidgetBase>>`, so direct
`erase`/`insert` is technically possible today, but there's no supported API for it.

A reconciler that diffs a new render against the previous one needs to remove widgets that
disappeared, replace ones whose type changed, and clear a subtree when a conditional branch
flips off — none of which the public API expresses today.

### Proposed API

```cpp
// include/Penumbra/Widgets/Box.h
void RemoveChild(WidgetBase* Child);
std::unique_ptr<WidgetBase> ReplaceChild(WidgetBase* Existing,
                                         std::unique_ptr<WidgetBase> Replacement);
void ClearChildren();
```

Generically useful beyond Iris — Pharos's collapsible tree view already destroys/rebuilds
subtrees by hand (`docs/penumbra_requirements.md` item 2); this gives it (and any future
consumer) a real API for that instead of reaching into the `Children` vector directly.

---

## 2. REQUIRED — child reordering

No way to change an existing child's position without removing and re-adding it (which loses
its place — it lands back at the end of the vector). A reconciler wants a cheap "just move
it" case for reordered-but-unchanged children (e.g. reordered list items).

### Proposed API

```cpp
void MoveChild(std::size_t FromIndex, std::size_t ToIndex);
WidgetBase* InsertChildAt(std::size_t Index, std::unique_ptr<WidgetBase> Child);
```

---

## 3. REQUIRED — a uniform way to enumerate a widget's children

`Box` exposes children via its public `Children` vector, but `SplitPanel` deliberately does
not use it — it has two named slots instead (`SetFirst`/`SetSecond`;
`include/Penumbra/Widgets/SplitPanel.h`: "Box's generic Children vector is unused here").
`WidgetBase` has no virtual accessor for children at all, so any generic tree-walking
consumer (a reconciler, but also e.g. a debug tree inspector) has to special-case `SplitPanel`
vs `Box` today.

### Proposed API

```cpp
// include/Penumbra/Widgets/WidgetBase.h
virtual std::size_t GetChildCount() const { return 0; }
virtual WidgetBase* GetChildAt(std::size_t Index) const { return nullptr; }
```

`Box` implements this over `Children`; `SplitPanel` implements it over `First`/`Second`.
Storage doesn't need to unify — just the read contract.

---

## 4. Minor — no identity/`key` concept needed in Penumbra itself

Noting for completeness: matching new elements to existing widget instances across
re-renders (React's `key` pattern) is naturally owned by the Iris runtime — an
Iris-side map from `key` → `WidgetBase*`, built from the pointers `AddChild`/etc. already
return. Penumbra doesn't need any identity concept of its own.

---

## 5. Explicitly not requested

- No changes to `Measure`/`Arrange`/`Draw`/`UpdateInteractionState`, the layout model, style
  structs, or the render primitive set — this is entirely about tree *mutation* shape, not
  rendering or layout.
- No built-in signals/observables inside Penumbra — the reactive model belongs in Iris's
  runtime; Penumbra stays an imperative, opinion-free tree, consistent with its existing
  "no defaults, no opinions" rule (`docs/penumbra_poc_spec.md`).

---

## 6. What unblocks when this lands

Penumbra becomes usable as the target of a real diff/patch-style renderer instead of only
supporting build-once-and-mutate-fields-in-place trees. Item 3 also benefits any future
generic tooling (tree inspectors, serialization, undo/redo) that needs to walk an arbitrary
widget tree without knowing every concrete widget type — not an Iris-only payoff.
