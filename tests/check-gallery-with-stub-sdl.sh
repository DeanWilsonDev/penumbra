#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
nox_compiler="${NOX:-nox}"
c_compiler="${NOX_CC:-${CC:-cc}}"
stub_libraries="$(mktemp -d)"
gallery_output="$(mktemp)"
trap 'rm -rf "${stub_libraries}" "${gallery_output}"' EXIT

stub_source="${repository_root}/tests/sdl-stub/stub.c"
${c_compiler} -shared -fPIC -DSTUB_CORE -o "${stub_libraries}/libSDL3.so" "${stub_source}" -lm
${c_compiler} -shared -fPIC -DSTUB_TTF -o "${stub_libraries}/libSDL3_ttf.so" "${stub_source}"
${c_compiler} -shared -fPIC -DSTUB_IMAGE -o "${stub_libraries}/libSDL3_image.so" "${stub_source}"

cd "${repository_root}/sdl"
LIBRARY_PATH="${stub_libraries}" "${nox_compiler}" build "$@"
cd "${repository_root}"
LD_LIBRARY_PATH="${stub_libraries}" build/penumbra-gallery > "${gallery_output}"

expect_line() {
    if ! grep -qF -- "$1" "${gallery_output}"; then
        echo "penumbra gallery: missing '$1' in its output:" >&2
        cat "${gallery_output}" >&2
        exit 1
    fi
}

expect_line 'stub TTF: rendered "presses: 1" at 30'
expect_line 'stub SDL: text input on at frame 8'
expect_line 'stub TTF: rendered "click anhd type here" at 30'
expect_line 'stub SDL: 41 frames,'
expect_line ', 0 live textures'
echo "penumbra gallery: ran 41 frames against the stub SDL"
