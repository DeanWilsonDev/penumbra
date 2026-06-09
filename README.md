# Penumbra (Proof of Concept)

A retained-mode C++20 UI framework on SDL3 (`SDL_Renderer` path). This is a
throwaway-grade PoC built to validate the layered architecture described in
[`penumbra_poc_spec.md`](penumbra_poc_spec.md).

## Layers

```
Demo App            stands in for Dawn; defines the Theme + resolvers
Penumbra::Widgets   Box, Button, Label, … (styled by values passed in)
Penumbra::Render    wraps SDL_Renderer; owns the IFontBackend (SDL_ttf)
Penumbra::Platform  the only code that calls SDL directly
```

The hard rule: no `SDL_Color` literal and no pixel literal anywhere under
`include/Penumbra/` or `src/Penumbra/`. All values live in the demo's `Theme`.

## Build & run

Requires CMake ≥ 3.24, a C++20 compiler, and SDL3 + SDL3_ttf installed with
their CMake config packages.

```bash
# macOS (Homebrew)
brew install sdl3 sdl3_ttf

# Debian/Ubuntu — needs SDL3 packages (e.g. libsdl3-dev libsdl3-ttf-dev),
# available on recent releases or from a backport/source build.
sudo apt install libsdl3-dev libsdl3-ttf-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/demo/penumbra_demo
```

The demo loads its font from `demo/assets/` (vendored JetBrains Mono Nerd Font;
see `demo/assets/FONT-README.md` for its origin and license).
