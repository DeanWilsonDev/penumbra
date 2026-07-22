# Penumbra — Next Steps

> Living backlog, not a session-changelog: add an entry for anything new
> and open; remove or mark an entry done the moment it's actually
> implemented, rather than letting finished work linger. First file of
> this kind in this repo (previously used `docs/*_requirements.md`, one
> file per investigation, still present as historical record — not
> migrated into here retroactively).
> Last updated: 2026-07-21.

## Open items

### `ColorBorderHovered`/`Pressed`/`Disabled` — border-color has no per-state override

Found continuing pharos-proto's Iris/Lustre componentization pass: its
`ColorFilterDropdown`'s closed trigger square (`DropdownTrigger` in
`pharos-proto/src/ui/color_filter_dropdown.cpp`) changes its *border* color
on hover/open —

```cpp
// pharos-proto/src/ui/color_filter_dropdown.cpp
Style.ColorBorder = (hovered || state->isOpen) ? theme->ColorAccent : theme->ColorPanelBorder;
```

— set by hand each frame in `UpdateInteractionState`, because there's
nowhere for a `:hover { border-color: ... }` Lustre rule to land. This is
now the one remaining thing blocking a full `.iris`/`.lustre` migration of
that trigger widget (its icon-drawing, the previously-blocking gap, is
already resolved now that `<Icon>` exists upstream).

`BoxStyle` (`include/Penumbra/Widgets/Styles.h:22-93`) has
`ColorBackgroundHovered`/`ColorBackgroundPressed`/`ColorBackgroundDisabled`
(lines 63-65, added for exactly this kind of per-state Lustre override —
see the doc comment at lines 57-62) and
`GradientTopHovered`/`GradientBottomHovered`/`GradientTopPressed`/
`GradientBottomPressed` (lines 45-48), but `ColorBorder` (line 24) is a
single flat field with no per-state sibling at all. `Box::BackgroundForState()`
(`src/Penumbra/Widgets/Box.cpp:308-319`) is what resolves the background
trio at draw/update time; there's no equivalent `BorderForState()`, and
`Box::Draw`'s border stroke (`Box.cpp:377-378`) reads `Style.ColorBorder`
directly, unconditionally.

**What would unblock this:** `ColorBorderHovered`/`ColorBorderPressed`/
`ColorBorderDisabled` fields on `BoxStyle`, mirroring the existing
`ColorBackgroundHovered`/`Pressed`/`Disabled` trio exactly (same
zero-alpha-means-no-override convention), plus a `BorderForState()` sibling
to `BackgroundForState()` that `Box::Draw` consults for its border stroke.
Lustre/`penumbra-ui-backend`'s own side of wiring `:hover`/`:active`/
`:disabled { border-color: ... }` through to these new fields is a separate
follow-up in `penumbra-ui-backend` once this lands here, the same layering
the `background-color` precedent (`penumbra-ui-backend/docs/
pseudo_class_plain_box_decision.md`) already used.

**Not urgent/blocking:** `DropdownTrigger` stays a hand-rolled `Box`
subclass in Pharos in the meantime — already working today via
composition, Pharos's own established pattern for custom interactive
widgets (see that repo's `CLAUDE.md`). Recorded so a future migration pass
has the concrete blocker on record, not because anything is broken today.

### Explicitly not requested

- **A `Disabled` gradient variant.** No known consumer wants one; adding it
  speculatively would be unused surface. Revisit only if a real consumer
  needs it.
