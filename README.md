# colonel-hd

An HD version of Sierra's *The Colonel's Bequest* (DOS, EGA, SCI0), running
on its own SCI0 interpreter written in C++23. The vector backgrounds are
re-rendered at high resolution; sprites stay original and are scaled by an
integer factor. Game logic behaves exactly as in the original.

**Status: milestone 9 (the playthrough), release candidate 0.9; iteration 2 in progress (`review/ITERATION2_PLAN.md`).**
- The whole game plays from the start to the ending. A scripted
  playthrough gives the same kernel calls, screens, actors, parser results
  and music as ScummVM from the boot to the last screen.
- In HD mode the backgrounds are vector art at any integer scale; sprites
  and text are drawn as enlarged original pixels.
- Music and sound on an emulated AdLib (Nuked OPL3) or Roland MT-32
  (mt32emu, with your own ROMs).
- The game's own dialogs save and restore, in a documented, versioned format.
- The milestone reports are in `docs/reports/`.

You need your own copy of the game (English DOS EGA release; the GOG
release works). No game files are included, and none are ever committed.

## Building

The supported compiler is upstream LLVM Clang 23.1.4 (clang-cl on Windows)
with CMake 3.28 or later and Ninja.

### Linux, in the pinned toolchain container (recommended)

    docker/build-image.sh                       # once; tags colonel-hd-toolchain:<hash>
    docker/run.sh cmake --workflow --preset release

The other presets are `debug`, `asan` (ASan + UBSan), `tsan`, `fuzz` and
`cross-aarch64` (ARM64 under qemu-user). Behind a TLS-intercepting proxy, set
`COLONEL_HD_EXTRA_CA=/path/to/ca-bundle.crt` for `docker/build-image.sh`.

### Windows

From a Visual Studio developer prompt with LLVM 23.1.4 first on `PATH`, run
`cmake --workflow --preset windows-release` (or `windows-debug`,
`windows-asan`).

## Running

    build/release/src/app/colonel-hd /path/to/game            # the game
    build/release/src/app/colonel-hd --viewer /path/to/game   # the room viewer of release 0.1

- **In the game:** F12 switches between HD and the original look, and F11
  or Alt+Enter toggles fullscreen.
- **The room viewer** shows every picture (HD or original, visual or
  priority map), every view with its animation, and every font.
- **Analysis:** the first launch analyses all pictures for HD, behind a
  progress bar, and keeps the result in `cache/`.
- **Files:** the program keeps `colonel-hd.ini`, `game/`, `rom/`,
  `saves/`, `cache/` and `files/` in one data folder: `--data <folder>`,
  else its own folder if that holds `colonel-hd.ini` or `game/`, else the
  per-user folder (`docs/USER-README.md`, "Where the program keeps its
  files"). From a build tree, `--data local` uses `local/`. The `hints/`
  folder is read next to the executable: in a build tree, copy or link it
  there, or the HD pictures are drawn without hints.
- **Saves:** the game's own File menu saves and restores, one file per slot
  in `saves/` (`colonel-hd-001.sav` to `colonel-hd-099.sav`,
  `docs/formats/save-format.md`); the files the game itself writes go to
  `files/`.
- **Configuration:** the keys of `colonel-hd.ini` (mode, scale, music,
  MT-32 ROMs, volume, saves, log) are listed in `docs/USER-README.md`, the
  user guide that ships with the binaries.
- **Help:** `colonel-hd --help` lists the keys and options.

No release has been published yet: build the program as above. The release
workflow (`.github/workflows/release.yml`) builds and tests the Linux and
Windows archives for a version tag and attaches them to a GitHub release.

The development room viewer writes the same images to PNG files
(`tools/room-viewer/README.md`):

    build/release/tools/room-viewer/sci-room-viewer /path/to/game pic 1 --map all --scale 2 build/pic1.png
    build/release/tools/room-viewer/sci-room-viewer /path/to/game pic 1 --hd --hints hints --scale 5 build/pic1-hd.png

## Checks

    scripts/pre-push.sh                                # everything below, the sanitizers, fuzzing and ARM64
    docker/run.sh scripts/check-format.sh              # clang-format
    docker/run.sh python3 scripts/check-repo-policy.py # no game data, no float
    docker/run.sh python3 scripts/check-hints.py       # hint files against their schema
    docker/run.sh python3 scripts/check-vendored.py    # vendored files against their recorded SHA-256
    docker/run.sh python3 scripts/check-fuzz-list.py   # every fuzz target is in the fuzzed list
    docker/run.sh python3 scripts/check-deps.py        # declared library dependencies match the includes
    python3 scripts/check-analyser-version.py          # HD picture goldens changed only with an analyser_version bump
    docker/run.sh scripts/run-clang-tidy.sh build/debug

## Documentation

- `docs/ARCHITECTURE.md`: modules, data flow, error handling, trade-offs
- `docs/SPEC_ADDENDA.md`: the customer's additions to the task spec
- `docs/ASSUMPTIONS.md`: every assumption and decision, including the pinned
  ScummVM revision
- `docs/formats/`: the resource, picture, view and font formats
- `docs/hd-pipeline.md`: how pictures become HD backgrounds
- `hints/README.md`: the scene-analysis hints and their format
- `tools/reference-scummvm/README.md`: the reference ScummVM build
- `docs/reports/`: milestone reports
- `THIRD_PARTY.md`: third-party components and their licences

## How it is developed

The project is built milestone by milestone against a written
specification, with ScummVM as the behavioural reference. The changes to
that specification are in `docs/SPEC_ADDENDA.md`, every decision is in
`docs/ASSUMPTIONS.md`, and each milestone ends with a report in `docs/reports/`.
Most of the code is written by Claude, Anthropic's AI model, through Claude
Code. See `CONTRIBUTING.md` before opening an issue or a pull request.

## Licence and trademarks

GPL version 3 or later (see `LICENSE`). The SCI logic is ported from
ScummVM, which is GPL v3 or later; `THIRD_PARTY.md` lists the ported code
and the vendored components.

This is an unofficial project, not affiliated with or endorsed by Sierra,
Activision or ScummVM. *The Colonel's Bequest* and Sierra are trademarks of
their respective owners. The repository contains no game data: you need
your own copy of the game.
