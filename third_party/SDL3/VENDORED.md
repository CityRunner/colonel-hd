# SDL3, vendored

- Upstream: https://github.com/libsdl-org/SDL
- Version: 3.4.18, tag `release-3.4.18`, commit
  `829a65d769d935c4852f8159e964312c0957260a` (2026-10-02)
- Licence: zlib (`LICENSE.txt`)

Copied unchanged: `CMakeLists.txt`, `cmake/`, `include/`, `src/`,
`wayland-protocols/`, `LICENSE.txt`, `README.md`, `CREDITS.md`. Left out,
as the build does not use them: `test/`, `examples/`, `docs/`,
`build-scripts/`, `android-project/`, `VisualC/`, `VisualC-GDK/`, `Xcode/`
and the other top-level files.

`SHA256SUMS` holds the SHA-256 of every vendored file as checked out
(`scripts/check-vendored.py` checks them, B-48). Checked on 2026-11-13
against a fresh clone of the tag: 1,496 of the 1,503 files are
byte-identical; 7 differ only in line endings, which git's checkout rules
set (`.gitattributes`: `*.bat` with CR LF, everything else LF; upstream has
`compile_shaders*.bat` with LF and `SDL_sysurl.m` and the two `.masm` files
with CR LF). The 7 `src/render/gpu/shaders/*.hlsl` shader sources are not
committed: SDL's own `.gitignore` in that folder hides them; the build
uses the generated shader headers next to them, which are committed.

The build sets SDL's revision string to `SDL-3.4.18-829a65d769d9`
(`cmake/SciThirdParty.cmake`, B-47) instead of SDL's `git describe` of
this repository; update it with the version.

To update, copy the same paths from the new release tag and record the new
version here, in `THIRD_PARTY.md` and in `docs/ASSUMPTIONS.md`.
