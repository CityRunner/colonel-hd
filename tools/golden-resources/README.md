# golden-resources

Golden outputs of all resources, the stage 5 safety net
(`review/BACKLOG_V2.md` B-18, scope of Q-02): one SHA-256 per image, so
that a refactoring that changes any pixel of any resource is caught. Local
only (it needs the game); the committed list is
`tests/golden/resources.txt`, made with the repository's hints.

    COLONEL_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/golden-resources/sci-golden-resources /game tests/golden/resources.txt --hints hints --check

Lines (`<kind> <id> <layer> <hash>`):

- `pic <n> visual1|priority1|control1`: the 1x maps of path 1;
  `original1`: the original-mode frame at 1x; `hd<N>`: the HD image and
  `hdpri<N>`: the HD priority map, N = 1 … 16, from the scenes the
  program analyses (`sci::platform::analyse`, both planes). The `hd1`,
  `hd2`, `hd5`, `hd10` and `original1` lines equal those of
  `tests/golden/pictures.txt`.
- `view <n> <loop> <cel> 1x|hd<N>`: every cel at 1x, and through the HD
  sprite path (`sci::render::compose`, HD mode, over an empty picture) at
  N = 1, 2, 5, 10.
- `font <n> <glyph> 1x|hd<N>`: every glyph, and as interface text composed
  at N = 1, 2, 5, 10.
- `cursor <n> 1x|hd<N>`: every cursor, and scaled as the program shows it
  (`scale_cursor`) at N = 1, 2, 5, 10.

`--only pictures|sprites` limits the run; `--check` prints every
difference by resource, layer and N and exits with 1. The layers are in
`layers.cpp`; `tests/golden/layers_test.cpp` (CI) checks on synthetic
resources that a one-pixel change changes each layer's hash. The output
is hashes only; never commit images of the game.
