# Repository instructions

This branch (`nox-proto`) is Penumbra written in Nox, with no C++. The C++ library and its
`DEVELOPING.md` live on the `proto` branch; read them there when porting a piece of it.

- The Nox sources are under `src/`, one top-level class or namespace per name, and `src/Demo/` is
  the headless demo. `nox.json` names them as one project.
- Build with the Nox prototype compiler (`DeanWilsonDev/nox-systems-language`, branch `proto`):
  `nox build` or `nox run` from anywhere in the checkout.
- `sdl/` is the SDL3 layer and its windowed gallery, a separate project (`sdl/nox.json`) so the
  headless build never links SDL. Cloud sessions have no SDL3: check it with
  `tests/check-gallery-with-stub-sdl.sh` (and `--checked`), which runs it against `tests/sdl-stub/`.
- Check every change with `tests/check-demo.sh` and `tests/check-demo.sh --checked`. Only record a
  new `tests/demo-expected.txt` after checking the changed values by hand.
- Something Nox is missing or gets wrong goes on the Nox board as a `[proto] ` card and is fixed in
  the compiler, not worked around here. Note the friction in `README.md`.
- Name things for what they hold or do, write no code comments, and keep documents in Markdown.
- Pull requests from `claude/...` branches target `nox-proto`.
