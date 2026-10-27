#!/usr/bin/env bash
set -euo pipefail

nox_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
nox_compiler="${NOX:-nox}"
output_directory="${nox_root}/../build-nox"
mkdir -p "${output_directory}"

library_sources=(
    Penumbra/Maths.nox
    Penumbra/Geometry.nox
    Penumbra/Render/Color.nox
    Penumbra/Render/Renderer.nox
    Penumbra/Render/Utf8.nox
    Penumbra/Platform/InputState.nox
    Penumbra/Platform/Clipboard.nox
    Penumbra/Widgets/Length.nox
    Penumbra/Widgets/Styles.nox
    Penumbra/Widgets/LayoutContext.nox
    Penumbra/Widgets/WidgetBase.nox
    Penumbra/Widgets/FocusState.nox
    Penumbra/Widgets/Positioning.nox
    Penumbra/Widgets/KeyDispatch.nox
    Penumbra/Widgets/Stacking.nox
    Penumbra/Widgets/Box.nox
    Penumbra/Widgets/Label.nox
    Penumbra/Anim/Animation.nox
    Penumbra/Widgets/Button.nox
    Penumbra/Widgets/Checkbox.nox
    Penumbra/Widgets/ScrollablePanel.nox
    Penumbra/Widgets/TextInput.nox
)
demo_sources=(
    demo/HeadlessBackends.nox
    demo/main.nox
)

cd "${nox_root}"
"${nox_compiler}" build "$@" "${library_sources[@]}" "${demo_sources[@]}" -o "${output_directory}/penumbra_nox_demo"
echo "${output_directory}/penumbra_nox_demo"
