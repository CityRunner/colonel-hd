# Stage 1a: codebase map

Audit of `main` at `a0422d6` ("Merge milestone 9: the playthrough (#14)"),
on branch `audit`, 2026-11-05. Read-only: nothing outside `review/` was
changed. Milestone 10's work (branch `milestone-10`, declined, not merged)
is out of scope.

How to read this file:
- **Verified** statements carry a `file:line` reference or name the command
  that was run. Line counts were measured with `find ... | xargs cat | wc -l`
  (all lines, comments and blanks included).
- **[not verified]** marks what was taken from documentation only.
- Section 8 lists observations (docs/code disagreements, risks). No fixes
  or recommendations at this stage.

---

## 1. Modules

### 1.1 Overview

Nine libraries under `src/`, one platform library under `src/app/platform`,
one executable (`sierra-hd`). Every library is a static library declared
with `sci_add_library` (`cmake/SciTargets.cmake:42-63`) and added in
dependency order in `CMakeLists.txt:35-44`.

| Module | Target | Purpose | Files / lines (cpp+hpp) |
|---|---|---|---|
| `src/sci-core` | `sci-core` | Error/`Result` (`std::expected`), `SCI_ASSERT`, `ByteReader`, FP-profile check, logging, exact rationals, SHA-256, MD5, strict JSON reader, `Executor` interface | 19 / 1,344 |
| `src/sci-resource` | `sci-resource` | RESOURCE.MAP, volumes, patch files, LZW/Huffman decompression (port of ScummVM) | 13 / 1,470 |
| `src/sci-pic` | `sci-pic` | PIC parser into `PicCommand`s and the exact 1x rasteriser ("path 1"), with draw trace and pair map | 13 / 1,744 |
| `src/sci-vector` | `sci-vector` | Path 2: planar graph, gap closing, faces, fallbacks, curve fitting, hints, stroke integrity, scene (de)serialisation | 15 / 4,820 |
| `src/sci-view` | `sci-view` | View/loop/cel decoding, SCI0 cursors | 4 / 334 |
| `src/sci-text` | `sci-text` | Fonts, vocabulary, grammar/parser, Said | 7 / 1,676 |
| `src/sci-audio` | `sci-audio` | `MidiDevice` interface, AdLib driver on Nuked OPL3, MT-32 driver on mt32emu, mixer/resampler | 13 / 2,159 |
| `src/sci-render` | `sci-render` | HD supersampling rasteriser, priority render, original mode, frame composition, acceptance and A1 integrity checks | 13 / 2,174 |
| `src/sci-vm` | `sci-vm` | SCI0 interpreter ported from ScummVM: segments, scripts, bytecode, GC, kernel, 1x graphics (ports, paint, text, animate, menu, controls, cursor, transitions), sound/music/MIDI parser, events, files, saves, VM state hash | 103 / 24,661 |
| `src/app/platform` | `sci-platform` | Window-free part of the app, shared with tools: thread executor, game-folder reading, release identification, config file, hints loading, analysis cache, HD pictures, game frame, save folder, MT-32 ROM discovery | 21 / 1,990 |
| `src/app/src` | `sierra-hd` (executable) | SDL3 window, input, presentation, audio output, room viewer, directory file store | 9 / 2,646 |

Other source trees: `tools/` 41 files / 6,653 lines (C++, Python, shell);
`tests/unit` 56 / 8,780; `tests/fuzz` 14 / 649; `tests/golden` 1 / 114;
`tests/threads` 1 / 131; `tests/support` 2 / 93; `scripts/` 9 / 604.
Vendored: `third_party/SDL3` 1,511 files (~638k C/C++ lines),
`third_party/mt32emu` 50 files (12,303), `third_party/nuked-opl3` 4 files
(1,692).

sci-vm is 24.7k of the ~42k lines of first-party library/app code (59 %).

### 1.2 Main types and entry points per module

- **sci-core**: `Error` (`src/sci-core/include/sci/core/error.hpp:27`),
  `ByteReader` (`byte_reader.hpp:18`), `Executor` / `SerialExecutor`
  (`executor.hpp:15`, `:30`), `check_fp_profile()` (`fp_profile.hpp:19`),
  `Rational` / `RationalPoint` (`exact.hpp:42`, `:127`), `JsonValue`
  (`json.hpp:29`), `LogCategory` {General, Resources, Vm} (`log.hpp:23`).
- **sci-resource**: `ResourceManager::open(std::vector<GameFile>)` and
  `load(ResourceId)` (`src/sci-resource/include/sci/resource/resource_manager.hpp:38,44,52`);
  `ResourceId` (`resource_id.hpp:55`), `MapEntry`, `VolumeEntryHeader`,
  `PatchCandidate`.
- **sci-pic**: `parse_pic` (`src/sci-pic/include/sci/pic/parse_pic.hpp:19`)
  → `std::vector<PicCommand>` (`pic_command.hpp:102`); `draw_picture`
  (`draw_pic.hpp:44`) → `PictureMaps` (`picture_maps.hpp:38`: visual,
  priority, control, pair) and `DrawTrace` (`draw_trace.hpp:43`).
- **sci-vector**: `build_scene` (`src/sci-vector/include/sci/vector/build_scene.hpp:106`)
  → `BuiltScene` (`:92`) holding a `Scene` (`scene.hpp:109`: faces, scan
  edges, strokes, ellipses, boxes, blocks); `PictureHints`
  (`hints.hpp:89`); `read_scene`/`write_scene` (`scene_io.hpp:26`).
- **sci-view**: `decode_view` (`src/sci-view/include/sci/view/view.hpp:38`),
  `Cursor` (`cursor.hpp:25`).
- **sci-text**: `decode_font` (`src/sci-text/include/sci/text/font.hpp:31`),
  `Vocabulary` (`parser.hpp:90`), parse tree / Said types (`parser.hpp:41-85`).
- **sci-audio**: `MidiDevice` (`src/sci-audio/include/sci/audio/midi_device.hpp:38`),
  `AdlibDriver` (`adlib.hpp:87`) over `Opl3` (`opl3.hpp:22`),
  `Mt32Driver` (`mt32_driver.hpp:98`) over `Mt32` (`mt32.hpp:65`),
  `Mixer` (`mixer.hpp:31`).
- **sci-render**: `render_hd` (`src/sci-render/include/sci/render/rasterize.hpp:66`),
  `render_priority` (`:70`), `SceneSampler` (`:21`), `render_original`
  (`original.hpp:17`), `compose` (`compose.hpp:97`), `check_acceptance`
  (`acceptance.hpp:43`), `check_integrity` (`integrity.hpp:97`).
- **sci-vm**: public: `Game` / `GameHost` (`src/sci-vm/include/sci/vm/game.hpp:78`, `:31`),
  `run_deterministic` (`interpreter.hpp:183`, defined
  `src/sci-vm/src/interpreter.cpp:294`), `FileStore` / `SaveStore`
  (`files.hpp:19`, `:60`), `InputEvent` and replay parser (`events.hpp:54`),
  `StateHash` (`state_hash.hpp:23`). Internal: `Machine`
  (`src/machine.hpp:125`, `Machine::run_vm` at `src/machine.cpp:827`),
  `Subsystems` (`subsystems.hpp:28`), `Graphics` (`graphics.hpp:34`),
  `Sound` (`sound.hpp:43`), `Music` (`music.hpp:125`, timer
  `music.cpp:169`), save format `write_save` / `read_save`
  (`save_game.cpp:68`, `:179`).
- **sci-platform**: `identify_game` (`src/app/platform/src/identify.cpp:91`),
  `read_game_folder` (`game_files.cpp:12`), `parse_config`
  (`config.cpp:103`, `Config` at `include/sci/platform/config.hpp:47`),
  `AnalysisCache` (`analysis_cache.hpp:94`, `scene()` at
  `analysis_cache.cpp:269`), `HdPictures` (`hd_pictures.hpp:41`),
  `GameFrame` (`game_frame.hpp:28`, `update` at `game_frame.cpp:34`),
  `ThreadExecutor` (`thread_executor.hpp:15`), `SaveFolder`
  (`save_folder.hpp:19`), `PictureHintSet` (`picture_hints.hpp:21`),
  `MusicChoice` (`mt32_roms.hpp:48`).
- **sierra-hd**: `main` (`src/app/src/main.cpp:1177`), `run`
  (`:1085`), `parse_options` (`:958`), `GameHost` implementation
  (`:614`), `Presenter` (`:202`), `open_game` (`:350`),
  `prepare_analysis` (`:406`), `open_audio` (`:997`),
  `write_end_state` (`:1034`); `Viewer` (`viewer.hpp:56`), `AudioOutput`
  (`audio_output.hpp:49`), `DirectoryFileStore` (`file_store.hpp:19`),
  key/cursor conversion (`game_screen.hpp`).

### 1.3 Dependency graph (from CMake, verified)

Declared edges (`DEPENDS` of `sci_add_library`, linked `PUBLIC`,
`cmake/SciTargets.cmake:59`):

```
sci-core      <- (none)                                        src/sci-core/CMakeLists.txt
sci-resource  -> core                                          src/sci-resource/CMakeLists.txt
sci-pic       -> core, resource                                src/sci-pic/CMakeLists.txt
sci-vector    -> core, pic                                     src/sci-vector/CMakeLists.txt
sci-view      -> core, resource                                src/sci-view/CMakeLists.txt
sci-text      -> core, resource                                src/sci-text/CMakeLists.txt
sci-audio     -> core, resource  (+ PRIVATE sci_nuked_opl3, sci_mt32emu)
sci-render    -> core, pic, vector, view
sci-vm        -> core, resource, pic, view, text, audio
sci-platform  -> core, resource, pic, vector, render, vm, audio (+ PUBLIC Threads::Threads)
sierra-hd     -> all ten libraries + SDL3::SDL3-static (PRIVATE)   src/app/CMakeLists.txt:10-12
```

Edges actually used by `#include "sci/<lib>/..."` (measured with
`grep -rhoE '#include "sci/[a-z]+/'` per module):

| Module | Includes from |
|---|---|
| sci-pic | core (no sci-resource include) |
| sci-view | core (no sci-resource include) |
| sci-text | core (no sci-resource include) |
| sci-render | core, pic, vector (no sci-view include) |
| sci-audio | core, resource |
| sci-vm | core, resource, pic, view, text, audio |
| sci-platform | core, resource, pic, vector, render, vm, audio |
| app | all |

So four declared edges are unused in code: sci-pic→sci-resource,
sci-view→sci-resource, sci-text→sci-resource, sci-render→sci-view (also
`grep -rn "resource::\|sci/resource"` over sci-pic/view/text and
`grep "view::"` over sci-render: no hits).

### 1.4 Enforced dependency rules

- A library may depend only on project libraries declared before it;
  unknown or later targets fail at configure time
  (`cmake/SciTargets.cmake:45-53`). The graph is acyclic by construction.
- `include/` is PUBLIC, `src/` PRIVATE per library
  (`cmake/SciTargets.cmake:57-58`). Exceptions made by tests:
  `sci-vector-tests` (`tests/unit/CMakeLists.txt:49`), `sci-vm-tests`
  (`:89`), `sci-app-tests`/`sci-audio-output-tests` (compile app sources
  directly, `:98`, `:110`) and `fuzz-save` (`tests/fuzz/CMakeLists.txt:76`)
  add private `src/` directories to their include path.
- `sci_add_library` accepts no external dependency; vendored libraries are
  linked by name only: Nuked OPL3 and mt32emu PRIVATE to sci-audio
  (`src/sci-audio/CMakeLists.txt:7`), SDL3 to `sierra-hd`
  (`src/app/CMakeLists.txt:12`) and `sci-audio-output-tests`.
- Whole project: upstream Clang only (`cmake/SciCompilerFlags.cmake:8-12`),
  no exceptions (`:36-45`), no `float`/`long double` and SPDX headers
  (`scripts/check-repo-policy.py`, `.cpp`/`.hpp` only), clang-tidy bans
  platform-dependent functions (`.clang-tidy`, per
  `docs/ARCHITECTURE.md` "Determinism"; the config itself [not read in
  detail]).
- Not enforced by any tool: that a declared edge is used (see 1.3).

---

## 2. Entry points

### 2.1 The program `sierra-hd`

`src/app/src/main.cpp`. Usage (`main.cpp:6-8`, help text `:73-100`):

    sierra-hd [--viewer] [game folder]
    sierra-hd [--replay <file>] [--seed <n>] [--fast] [--end-state <file>] [game folder]
    sierra-hd --version | --help

`main` (`:1177`) handles `--version`, parses options, installs the assertion
handler and log sink, seeds `std::srand(1)` for mt32emu (`:1190-1194`),
checks the FP profile (`:1196`), `SDL_Init(SDL_INIT_VIDEO)`, then `run`
(`:1085`): reads `sierra-hd.ini` next to the executable (`:1087`), opens
and identifies the game, prepares the analysis cache (`cache/`, `:1125`),
then either the room viewer (`Viewer`) or the game (`sci::vm::Game` with
the `GameHost` at `:614`, files in `files/` `:1137`, saves via
`SaveFolder`).

### 2.2 Tools (`tools/`, never linked into the program; `tools/CMakeLists.txt`)

| Executable / script | Purpose | Needs game | README |
|---|---|---|---|
| `sci-resource-dump` | dump every resource via sci-resource | yes | none |
| `resource-compare/run.sh`, `compare.py` | M1 byte comparison with patched ScummVM | yes + reference | yes |
| `sci-graphics-dump` | dump pictures (variants), views, fonts at 1x | yes | none |
| `graphics-compare/run.sh`, `compare.py` | M2 bit-exact comparison with ScummVM | yes + reference | yes |
| `sci-room-viewer` | pictures/views/fonts to PNG, `--hd` frames | yes | yes |
| `sci-hd-report` | M3 replacement report and acceptance check | yes | yes |
| `sci-golden-hashes` | write/check `tests/golden/pictures.txt` | yes | yes |
| `sci-hint-tool`, `make_hints.py` | propose/check/build hint files | yes | yes |
| `sci-vm-trace` + `run-scenario.sh`, `run-all-scenarios.sh`, `run-round-trips.sh`, `compare*.py`, `make-parser-scenarios.py` | deterministic VM run, traces, screens, actors, parser, layout, OPL/MIDI, state hashes, saves; comparison with the reference | yes (+ reference for comparisons) | yes |
| `sci-sound-render` | render every sound through the music player + AdLib, OPL hash; `tests/golden/sounds.txt` | yes | yes |
| `sci-hd-frames` + `check-interface.sh` | headless frames at chosen ticks (original vs HD), composition timing, interface check | yes | yes |
| `playthrough/check.sh`, `program-run.sh`, `act-saves.sh`, `where.py` | M9 playthrough: vs reference, in the program under Xvfb, act saves | yes (+ reference for `check.sh`) | none (documented in `docs/playthrough.md`) |
| `audio-gaps/find_gaps.py` | find underruns in an audio capture | capture | yes |
| `reference-scummvm/build.sh`, `run.sh`, `REVISION` | build/run the pinned, patched ScummVM; CMake target `reference-scummvm` when `SCI_SCUMMVM_SOURCE` is set (`tools/CMakeLists.txt:20-29`) | ScummVM checkout | yes |
| `scummvm-patches/0001..0003` | dump resources, dump graphics, deterministic trace (139, 277, 1,852 lines) | — | yes |
| `common/` (`sci_tools_common`) | game opening, PNG writer, picture drawing, interface check; links sci-platform | — | none |

### 2.3 Tests and fuzzers

CTest targets (section 6). Fuzzers: 12 libFuzzer entry points in
`tests/fuzz/` (section 6.2), run by `scripts/run-fuzzers.sh` in the fuzz
preset or as corpus replays otherwise.

### 2.4 Scripts (`scripts/`)

`pre-push.sh` (local gate), `check-format.sh`, `check-repo-policy.py`,
`check-hints.py`, `run-clang-tidy.sh`, `run-fuzzers.sh`,
`package-release.sh` (release archives), `ci-minutes.py` (CI cost
measurement), `gen-trigonometry.py` (generates the sin/cos table of
glibc values used by sci-vm [not verified which file it writes]).

---

## 3. Data flows

### 3.1 Game files → resources

`read_game_folder` (`src/app/platform/src/game_files.cpp:12`) reads the
folder's files into memory as `GameFile`s; `identify_game`
(`identify.cpp:91`) identifies the release by resource hashes;
`ResourceManager::open` indexes map, volumes and patch files in ScummVM's
order; `load(id)` decompresses one resource on demand. No resource cache in
sci-resource (`docs/ARCHITECTURE.md` "Resources"; the VM caches).

### 3.2 Path 1 (1x pictures, views, fonts)

PIC bytes → `parse_pic` → `PicCommand`s → `draw_picture` → `PictureMaps`
(visual, priority, control, pair) + `DrawTrace`. Views → `decode_view`
(cels with mirroring applied); fonts → `decode_font`. Pure functions of
input bytes. Path 1 is what the game logic reads (priority/control maps).

### 3.3 Vector path (scene, hints)

`PictureMaps` + `DrawTrace` + `PictureHints` (from `hints/pic.NNN.json`,
87 files, loaded via `sci-platform`'s `PictureHintSet`) →
`build_scene` per plane (visual, priority) → `Scene`. The scene is cached
on disk by `AnalysisCache` (`cache/` next to `sierra-hd.ini`), key = game
data + analyser version + hint file + variant
(`docs/formats/analysis-cache.md`; `analysis_cache.cpp:90-300`). On
launch missing entries are analysed on the executor behind a progress bar
(`main.cpp:406`).

### 3.4 HD render → presentation

`HdPictures` (`hd_pictures.cpp:62-128`) renders the cached scenes of the
current picture stack at N with `render_hd` / `render_priority`
(rows in parallel through `ThreadExecutor`). Per frame, `GameFrame::update`
(`game_frame.cpp:34`) calls `sci::render::compose` for the 1x pixels whose
shown data changed: HD background where the picture drew, N×N blocks for
interface, sprites scaled N×N and occluded by the HD (or 1x) priority map.
The app's `Presenter` (`main.cpp:202`) uploads dirty rectangles to an SDL
streaming texture and draws it into an integer viewport. Original mode:
every pixel an EGA block (`render_original`).

### 3.5 The VM

Scripts (resources) → `Machine::run_vm` (bytecode) → `callk` via kernel
table (`kernel.cpp`, signature checks, workarounds) → `k_*.cpp` kernel
functions → `Subsystems`: `Graphics` (1x screen maps, ports, paint via
sci-pic, views via sci-view, text via sci-text fonts, animate, menu,
controls, cursor, transitions), `Sound`/`Music` (virtual-clock timer every
4 ms, MIDI parser, to a `MidiDevice`), `EventQueue`, parser (sci-text),
`Files` (host `FileStore`), saves (`write_save`/`read_save` to a host
`SaveStore`, `SaveFolder` in the app). Two drivers:
`run_deterministic` (tools; virtual clock, replay, traces) and `Game`
(program; `GameHost::wait` presents `Game::screen()`, passes input,
`Game::pass_wait` runs due music timer calls).

### 3.6 Audio

`Music::send` (`music.cpp:184`) → `MidiDevice` (`AdlibDriver` → `Opl3`
on Nuked OPL3, or `Mt32Driver` → `Mt32` on mt32emu with user ROMs; silent
when music is off) → in the program `AudioOutput` (`src/app/src/audio_output.cpp`)
wraps the device, mixes/resamples with `sci::audio::Mixer` on the main
thread (`:105-170`) and hands frames to SDL's audio thread with
`SDL_PutAudioStreamData` (`:216`) on a stream from
`SDL_OpenAudioDeviceStream` (`:76`); underruns counted (`:23`).

### 3.7 Configuration, cache, saves

`sierra-hd.ini` next to the executable (`main.cpp:1087`), keys `format`,
`game_folder`, `mode`, `scale`, `priority_snap`, `mt32_rom`, `music`,
`volume`, `saves` (`config.cpp`; `Config` at `config.hpp:47`). Hints:
`hints/` next to the executable (`main.cpp:394`). Cache: `cache/`
(`main.cpp:1125`). Game-written files: `files/` (`main.cpp:1137`). Saves:
`saves/` (default) as `sierra-hd-001.sav`..`-099.sav` (`save_folder.cpp:39`,
format `docs/formats/save-format.md`).

---

## 4. Build system

- **CMake** ≥ 3.28, Ninja, C++23 (`cxx_std_23`, no extensions,
  `cmake/SciTargets.cmake:17-18`). Options (`CMakeLists.txt:13-19`):
  `SCI_WARNINGS_AS_ERRORS` (ON), `SCI_BUILD_TESTS`, `SCI_BUILD_TOOLS`,
  `SCI_BUILD_APP`, `SCI_DEBUG_HIGHLIGHT`, `SCI_WITHOUT_STROKE_INTEGRITY`;
  plus `SCI_SANITIZE`, `SCI_FUZZ` (`cmake/SciCompilerFlags.cmake:53-55`),
  `SCI_SCUMMVM_SOURCE` (`tools/CMakeLists.txt:20`).
- **Presets** (`CMakePresets.json`): `debug`, `release`, `asan`
  (RelWithDebInfo, address;undefined), `tsan` (RelWithDebInfo, thread),
  `fuzz` (RelWithDebInfo, `SCI_FUZZ=ON`), `cross-aarch64` (Release,
  aarch64 toolchain `cmake/toolchains/aarch64-linux-gnu.cmake`,
  `SCI_BUILD_APP=OFF`, tests under qemu-user), `windows-debug`,
  `windows-release`, `windows-asan` (clang-cl, lld-link). Build, test
  (`outputOnFailure`, `noTestsAction=error`) and workflow presets for each
  except `fuzz` (configure + build only).
- **Toolchain container** (`docker/Dockerfile`): base `ubuntu:noble` pinned
  by digest, apt from the frozen `noble` release pocket, upstream LLVM
  23.1.4 downloaded from GitHub releases and checked by SHA-256 per arch;
  amd64 image carries the arm64 cross sysroot and `qemu-user`. Tag =
  hash of the Dockerfile (`docker/image-tag.sh`). `docker/run.sh` mounts the
  repo at `/work`, optional read-only mounts (`SIERRA_HD_MOUNTS`), ccache
  with `SIERRA_HD_CCACHE=1`.
- **Compiler flags** (`cmake/SciCompilerFlags.cmake`, global, vendored
  code included):
  - FP profile: `-ffp-contract=off` (also for clang-cl) and `-fno-fast-math`
    (`:33-34`); checked at run time by `check_fp_profile()` on the main
    thread (`main.cpp:1196`) and in every worker
    (`thread_executor.cpp:24`, `background_task.hpp:41`).
  - `-fno-exceptions` / `/EHs-c-` + `_HAS_EXCEPTIONS=0` (`:36-45`).
  - `-fuse-ld=lld` except with clang-cl and on Apple, i.e. on Linux (`:47-50`).
  - Sanitizers: `-fsanitize=<list> -fno-omit-frame-pointer
    -fno-sanitize-recover=all` (`:88-89`); clang-cl: ASan only (`:67-86`);
    `SCI_FUZZ` forces ASan+UBSan plus `-fsanitize=fuzzer-no-link` (`:57-63`).
  - `-fwrapv` only in Release/RelWithDebInfo builds without sanitizers
    (`:91-96`), so Debug builds have no `-fwrapv` either.
  - `_GLIBCXX_ASSERTIONS` and libc++ extensive hardening in Debug and all
    sanitizer builds (`:98-105`).
  - Warnings per project target: `-Wall -Wextra -Wpedantic -Wconversion
    -Wsign-conversion -Wshadow -Wold-style-cast ...` + `-Werror`
    (`cmake/SciTargets.cmake:20-39`); not applied to vendored code
    (mt32emu built with `-w`, `cmake/SciThirdParty.cmake:64`).
  - `sierra-hd` on Linux links `-static-libstdc++ -static-libgcc` unless
    sanitized (`src/app/CMakeLists.txt:19-21`).
- **CI** (`.github/workflows/ci.yml`; tiers in its header `:1-19`):
  - triggers: pull requests to `main` (not docs/reports/`*.md`-only
    changes) and `workflow_dispatch`; pushes to branches run nothing.
  - `plan` job computes the tier (`:62-135`).
  - `hints` (hint-only changes): `scripts/check-hints.py`.
  - `lint`: clang-format, repository policy, hint check, clang-tidy on
    the debug configuration (`:148-171`).
  - Draft PR: `wip-asan` (`cmake --workflow --preset asan`, all tests) then
    `wip-tsan` (only `sci-thread-tests`, `ctest -L threads`).
  - Manual `quick`: release workflow on Linux x64.
  - Ready-for-review PR (gate): `linux` matrix {x64, arm64 native runners}
    × {release, asan, tsan}, each `cmake --workflow` (all CTest tests);
    `fuzz` (fuzz preset, 60 s per target, cached grown corpus); `windows`
    x64 (windows-release + windows-asan) and arm64 (windows-release), LLVM,
    Ninja and ccache downloaded and SHA-256-checked; macOS (release, asan,
    tsan) only by manual dispatch.
  - Actions pinned by commit SHA.
- **Release** (`.github/workflows/release.yml`): on `v*` tags; Linux
  x64/arm64, Windows x64/arm64, macOS arm64 release builds with tests, then
  `scripts/package-release.sh` and a publish job.
- **Local pre-push** (`scripts/pre-push.sh`): in the container, stop at
  first failure: format + policy + hints + clang-tidy; release workflow;
  (unless `--quick`) asan, tsan workflows; fuzz 30 s/target;
  `cross-aarch64` workflow under qemu-user. Game-file checks are separate
  (`pre-push.sh:16-18`).

---

## 5. External dependencies

| Component | Version / pin | Licence | Where | Used by |
|---|---|---|---|---|
| SDL3 | 3.4.18, commit `829a65d7…` | zlib | `third_party/SDL3` (subset; `VENDORED.md`) | `sierra-hd` (window, input, presentation **and audio output**), `sci-audio-output-tests` |
| Nuked OPL3 | 1.8, commit `765ec962…` | LGPL-2.1+ | `third_party/nuked-opl3` | sci-audio (private) |
| mt32emu (Munt) | libmt32emu 2.8.3, commit `3b05ec27…` | LGPL-2.1+; `sha1/` BSD-3 | `third_party/mt32emu` (synth core only, own `config.h`) | sci-audio (private) |
| ScummVM (reference only) | `ed949213a23440447ea402e263daa871394b6bf9` (`tools/reference-scummvm/REVISION`, `docs/ASSUMPTIONS.md:32`) | GPL-3.0+ | outside the repo; `build.sh` refuses another revision (`build.sh:23-30`) | tools only, with patches 0001-0003 |
| LLVM | 23.1.4 | Apache-2.0 w/ LLVM exc. | Docker image / CI downloads, SHA-256 pinned | build |
| Ubuntu 24.04 packages | frozen release pocket | various | Docker image | build (incl. `libsdl2-dev`, `libpng-dev` [use not verified]) |
| GitHub Actions | pinned by SHA | — | `.github/` | CI |

Versions and licences come from `THIRD_PARTY.md` and the `VENDORED.md`
files; per-file SHA-256s are listed there. Whether the vendored files
still match those SHA-256s was **not verified** (no script checks them;
`grep VENDORED scripts/` finds nothing).

Fetched at build time: nothing by CMake (no `FetchContent`,
`ExternalProject` or `file(DOWNLOAD)`: `grep` over all CMake files). The
Docker image build downloads LLVM and apt packages; the CI Windows/macOS
jobs download LLVM, Ninja and ccache (SHA-256-checked); the reference
ScummVM is cloned by hand.

Ported code: ScummVM GPL-3.0+ files listed in `THIRD_PARTY.md`
(sci-resource, sci-pic, sci-view, sci-text, sci-vm (68 files),
`game_screen.cpp`).

---

## 6. Test coverage

### 6.1 CTest targets (built with `SCI_BUILD_TESTS`, harness `tests/support`)

All run in every CI preset (Linux x64/arm64 release, asan, tsan; Windows;
cross-aarch64 locally); none needs the game files. Test counts are
`SCI_TEST(` occurrences.

| Target | Sources | What it tests | Tests |
|---|---|---|---|
| `sci-core-tests` | `tests/unit/core/*` | ByteReader, Error, exact rationals, FP profile, JSON, log, MD5, SHA-256 | 21 |
| `sci-resource-tests` | `tests/unit/resource/*` | decompression, patch files, ids, manager, map, volume on synthetic data | 28 |
| `sci-pic-tests` | `tests/unit/pic/*` | parser and rasteriser on built PICs | 23 |
| `sci-view-tests` | `tests/unit/view/*` | views, cursors | 6 |
| `sci-text-tests` | `tests/unit/text/*` | fonts, parser/Said | 13 |
| `sci-vector-tests` | `tests/unit/vector/*` | scene building, differential rasteriser vs reference sampler, fitting, hints, scene I/O, stroke integrity | 34 |
| `sci-audio-tests` | `tests/unit/audio/*` | AdLib, mixer, MT-32 driver, MT-32 (skipped without `SIERRA_HD_MT32_ROMS`), OPL3 | 46 |
| `sci-render-tests` | `tests/unit/render/*` | composition, rendering | 14 |
| `sci-platform-tests` | `tests/unit/platform/*` + `tools/common/interface_check.cpp` | analysis cache, game frame, interface check, MT-32 ROMs, config/identify, save folder | 31 |
| `sci-vm-tests` | `tests/unit/vm/*` | files, input, music device, music, path matching, save kernel, saves, screen, script patcher, state (on synthetic machines) | 106 |
| `sci-app-tests` | `tests/unit/app/game_screen_test.cpp` + `game_screen.cpp` | key and cursor conversion | 11 |
| `sci-audio-output-tests` (only with `SCI_BUILD_APP`) | `audio_output_test.cpp` + `audio_output.cpp` | hand-off to SDL audio thread, dummy driver; label `threads` | 2 |
| `sci-golden-tests` | `tests/golden/synthetic_test.cpp` | golden hashes of synthetic scenes (same constants on x86-64 and ARM64) | 2 |
| `sci-thread-tests` | `tests/threads/threads_test.cpp` | ThreadExecutor, parallel rendering vs serial; label `threads` | 3 |
| 12 × `fuzz-*-corpus` | `tests/fuzz/*` | replay of the hand-made seed corpora: byte-reader, decompress, game-folder, pic, view-font, vector, json, hints, replay, scene, parser, save | — |

### 6.2 Fuzzing

12 libFuzzer targets (`tests/fuzz/CMakeLists.txt`). CI gate: 60 s per
target with a cached, growing corpus (`ci.yml` `fuzz` job); pre-push 30 s.
Otherwise each target replays its seed corpus as a CTest test
(`cmake/SciTesting.cmake:31-37`). Not fuzzed: sci-vm bytecode/kernel
execution, audio drivers, composition, analysis cache files (the `scene`
target covers `read_scene`) [inferred from the target list].

### 6.3 Local-only tests (need the game files; never in CI)

| Check | How | Reference needed |
|---|---|---|
| Resources (M1) | `tools/resource-compare/run.sh` | patched ScummVM |
| Pictures, views, fonts at 1x (M2) | `tools/graphics-compare/run.sh` | patched ScummVM |
| HD acceptance (M3) | `sci-hd-report` | no |
| Golden hashes of every picture (475 lines: HD at N=1,2,5,10 and original) | `sci-golden-hashes … --check tests/golden/pictures.txt`, also on ARM64 under qemu-user | no |
| Sound golden hashes (119 lines, OPL output of every sound) | `sci-sound-render <game> --golden tests/golden/sounds.txt --check` | no |
| Hint anchors | `sci-hint-tool <game> check hints` (schema check runs in CI) | no |
| 39 scenario runs (non-comment lines of `tests/scenarios/scenarios.txt`; 36 replay files): kernel trace, screens, 1x maps, actors, parser, layout, OPL (and MIDI for MT-32) must match | `tools/vm-trace/run-all-scenarios.sh` | patched ScummVM |
| Save round trips (M8): state hashes equal with/without save+load at 5 ticks | `tools/vm-trace/run-round-trips.sh` | no |
| Interface check (M6b) per frame and N | `tools/hd-frames/check-interface.sh` | no |
| Playthrough vs reference, 43 checkpoints | `tools/playthrough/check.sh` | patched ScummVM |
| Playthrough in the program (Xvfb, `--fast`, end state = sci-vm-trace's) | `tools/playthrough/program-run.sh` | no |
| MT-32 tests | `SIERRA_HD_MT32_ROMS=… sci-audio-tests` | user's ROMs |

Results of these were not re-run for this audit except the coverage run
below; the milestone 9 report claims they all pass (`reports/milestone-9.md:66-122`)
[not verified].

### 6.4 Line coverage (measured)

**Method.** Clang source-based coverage, Debug, in the toolchain
container (`docker/run.sh`), scratch build `build/audit-coverage/b`
(git-ignored): `-fprofile-instr-generate -fcoverage-mapping`,
`SCI_BUILD_TOOLS=OFF`, `SCI_BUILD_APP=OFF` (so `sci-audio-output-tests`
and SDL are out), `SCI_WARNINGS_AS_ERRORS=OFF` (no warning appeared:
`grep -c warning: build.log` = 0). `ctest -j4`: 25/25 passed in 72 s.
Profiles merged with `llvm-profdata merge -sparse`; `llvm-cov report` over
all 25 test binaries, excluding `third_party/`, `tests/` and system headers.
llvm-cov warned "187 functions have mismatched data" (inline header
functions compiled differently across binaries); the effect on the totals
is small but not quantified.

**CI test suite (unit + golden + threads + fuzz corpus replays)**: lines
58.7 % (24,362 / 10,066 missed), functions 66.1 %, branches 51.4 %.

| Module | Lines | Line cover | Function cover | Branch cover |
|---|---|---|---|---|
| sci-core | 698 | 83.2 % | 86.1 % | 76.7 % |
| sci-resource | 669 | 93.0 % | 96.2 % | 80.2 % |
| sci-pic | 813 | 87.8 % | 96.4 % | 80.7 % |
| sci-vector | 3,396 | 89.6 % | 96.3 % | 78.8 % |
| sci-view | 140 | 88.6 % | 100 % | 72.6 % |
| sci-text | 1,033 | 86.5 % | 95.5 % | 76.8 % |
| sci-audio | 1,040 | 81.2 % | 79.6 % | 73.1 % |
| sci-render | 1,275 | 95.3 % | 97.2 % | 81.6 % |
| **sci-vm** | **14,101** | **38.4 %** | **45.9 %** | **27.8 %** |
| sci-platform | 921 | 62.3 % | 68.8 % | 62.8 % |
| app (`game_screen.cpp` only) | 152 | 100 % | 100 % | 96.2 % |

(Lines here are llvm-cov's executable lines, not file lines.)

Files linked into no test binary (so not in the report, effectively 0 %):
`src/app/src/main.cpp`, `viewer.cpp`, `file_store.cpp`,
`audio_output.cpp` (tested only with `SCI_BUILD_APP`, excluded here),
`src/app/platform/src/hd_pictures.cpp`, `game_files.cpp`,
`src/sci-render/src/acceptance.cpp`, `src/sci-vm/src/interpreter.cpp`
(`run_deterministic`), `scan.cpp`, and the two `component.cpp`.

sci-vm files with the most uncovered lines in CI: `machine.cpp` (bytecode
interpreter) 2.5 % of 1,223; `gfx_menu.cpp` 0 % of 750;
`k_graphics.cpp` 0 % of 437; `gfx_animate.cpp` 0 % of 413;
`gfx_controls.cpp` 0 %; `k_strings.cpp`, `k_movement.cpp`, `k_lists.cpp`,
`k_scripts.cpp`, `k_math.cpp`, `gfx_compare.cpp`, `gfx_cursor.cpp`,
`object.cpp`, `gc.cpp` 0 %; `gfx_paint.cpp` 3.8 %; `gfx_text.cpp` 2.9 %.
The interpreter itself is exercised only by the local, game-dependent
checks of 6.3.

PLAYTHROUGH_PLACEHOLDER

---

## 7. Documentation map

`docs/ARCHITECTURE.md` (modules, flows, error handling, trade-offs),
`docs/hd-pipeline.md`, `docs/hd-composition.md`, `docs/playthrough.md`,
`docs/formats/*` (resources, pictures, views-and-fonts, analysis-cache,
replay-and-trace, save-format), `docs/SPEC_ADDENDA.md`,
`docs/ASSUMPTIONS.md` (2,895 lines, decision log), `docs/USER-README.md`,
`reports/milestone-*.md` (0 to 9), a README in every `src/` library and in
most tools, `tests/README.md`, `hints/README.md`, `THIRD_PARTY.md`.

---

## 8. Observations

Facts found while mapping; no fixes or recommendations here. Severity per
the brief only where clearly warranted.

OBSERVATIONS_PLACEHOLDER
