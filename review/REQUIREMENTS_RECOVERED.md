# Stage 1b: requirements recovered from the code

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-05.
Read-only: nothing outside `review/` was changed. Milestone 10 (declined,
not merged) is out of scope.

How to read this file:
- **R-xx** is a requirement the code on `main` *actually implements*,
  recovered from the code and its tests. Docs were used only as pointers
  to where to look; where a doc claims something the code does not do, it
  is listed as an observation in section V, not as a requirement.
- **FACT** carries a `file:line` reference to the code that implements it,
  and where one exists the test that checks it (`test file:line`). Paths
  are relative to the repository root; `main.cpp` is
  `src/app/src/main.cpp`.
- **HYP** (hypothesis) carries a confidence (high / medium / low) and the
  rationale. It was not verified.
- **Tested**: `CI` = a CTest test that runs in every CI preset
  (`review/MAP.md` 6.1; Linux x64/arm64 release/asan/tsan, Windows, fuzz
  corpus replays); `local` = only a game-dependent check run by hand
  (MAP.md 6.3: needs the customer's game files, and for some the patched
  reference ScummVM); `untested` = no test found (MAP.md 6.4 lists the
  files linked into no test binary). "local (claimed)" means the milestone
  report says the check passes; not re-run for this audit.
- IDs are stable. Sections are letters; numbering is continuous.

Summary counts are at the end (section W).

---

## A. Game folder identification and supported releases

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-01 | The game folder must contain `RESOURCE.MAP` (file names compared case-insensitively); otherwise the program refuses it with a message that names the file. | FACT | `src/app/platform/src/identify.cpp:72-77`, `:92-96`; test `tests/unit/platform/platform_test.cpp:71-74` | CI |
| R-02 | The one *tested* release (English DOS 3.5" 1.000.046) is identified by size + SHA-256 of `RESOURCE.MAP` and `RESOURCE.001`-`.004`. If the map matches but any volume is missing or differs, the folder is refused as "damaged or modified". | FACT | `identify.cpp:25-51`, `:97-105` | local (positive case needs the game; no CTest) |
| R-03 | The patch files `SCRIPT.052` and `TEXT.052` are checked by size + SHA-256; missing or different gives a warning (log + message box), never a refusal. | FACT | `identify.cpp:53-64`, `:111-121`; `main.cpp:374-377` | untested |
| R-04 | The English DOS 5.25" release is accepted, recognised by ScummVM's detection entry (MD5 of the first 5000 bytes of `RESOURCE.MAP`, map size 12030), with a warning that it is untested and that the HD hints are not used. | FACT | `identify.cpp:66-70`, `:124-135` | untested |
| R-05 | Any other folder is refused (`ErrorCode::Unsupported`, "not a supported release"). | FACT | `identify.cpp:136-138`; test `platform_test.cpp:75-79` | CI |
| R-06 | The identified release has no effect beyond the messages: `Release::Floppy525` is read nowhere outside `identify.cpp`. Hints are kept off other data only by the PIC SHA-256 stored in each hint file (R-31). | FACT | `grep -rn Floppy525 src tools` → only `identify.hpp:18`; `src/app/platform/src/picture_hints.cpp:31-38` | — (see V-01) |
| R-07 | The interpreter's own files (SCIV.EXE, drivers) are not needed and not checked. | FACT | `identify.cpp:23-24` (only resource and patch files are listed) | n/a |
| R-08 | The whole game folder is read into memory (`GameFile` list) before identification; the resource manager and the game's file reads work on these copies. | FACT | `src/app/platform/src/game_files.cpp:12`; `main.cpp:363`, `:378-380` | untested (`game_files.cpp` in no test binary); fuzz target `fuzz_game_folder` (corpus replay in CI) |
| R-09 | If no game folder is configured or given, the program asks for one with the system folder dialog; cancelling ends the program with an error naming the configuration file. | FACT | `main.cpp:351-362` | untested |

## B. Resources and decompression

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-10 | Only the SCI0 resource map format is accepted: format detection as ScummVM's `ResourceManager::init` (the map, then the first volume in name order); later formats and a map naming a missing volume are refused. | FACT | `src/sci-resource/src/resource_manager.cpp:95-115`; tests `tests/unit/resource/resource_map_test.cpp:44-54`, `volume_test.cpp:57` | CI |
| R-11 | Volumes are ordered by name as ScummVM sorts them (case-sensitive); a map entry whose offset is beyond its volume is skipped with a warning (ScummVM's `validateResource`); the first entry of an id wins. | FACT | `resource_manager.cpp:73`, `:131-151` | CI (`resource_manager_test.cpp:66-112`) |
| R-12 | Patch files override map entries, scanned after the map in ScummVM's order; a patch whose header is not valid is skipped (debug log). | FACT | `resource_manager.cpp:117-121`, `:154-171`; tests `patch_file_test.cpp:13-32`, `resource_manager_test.cpp:95` | CI |
| R-13 | Compression methods 0 (stored), 1 (LZW) and 2 (Huffman) are decoded; 3, 4, 18-20 are refused as "not used by SCI0 games"; other values are invalid data. | FACT | `src/sci-resource/src/volume.cpp:100-116`; tests `decompress_test.cpp:49-110`, `volume_test.cpp:47` | CI; fuzzed (`fuzz_decompress`) |
| R-14 | Decompression is a port of ScummVM's at the pinned revision, including its edge cases (header size arithmetic wrapping as `uint32_t`; an LZW stream that ends early is zero-filled). | FACT | `volume.cpp:65-69`, `:86`; test `decompress_test.cpp:85` | CI |
| R-15 | Every resource of the tested release decompresses byte-identically to the reference ScummVM's dump. | HYP high — `tools/resource-compare/run.sh` exists for this (milestone 1) and the milestone report claims it passes; not re-run | `tools/resource-compare/` | local (claimed) |
| R-16 | `sci-resource` keeps no cache: every `load` reads and decompresses again (callers cache). | FACT | `resource_manager.cpp:186-200` (no cache member used) | CI |

## C. Pictures, views, fonts, cursors at 1x (path 1)

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-17 | PIC resources are parsed into commands covering all SCI0 opcodes 0xF0-0xFF and the extended opcodes used by SCI0 (palette entries, palette, embedded view, priority table); malformed pictures are rejected with an error. | FACT | `src/sci-pic/src/parse_pic.cpp:44-74`; tests `tests/unit/pic/parse_pic_test.cpp:24-102` | CI; fuzzed (`fuzz_pic`) |
| R-18 | The 1x rasteriser reproduces Sierra's/ScummVM's drawing: Bresenham lines, dithered colours as pairs, flood fill over white with Sierra's abort rules (fills leak through gaps), circle/box patterns wrapping at the right edge, mirroring, 4 EGA palettes, overlays, priority tables. Output: visual, priority, control and pair maps. | FACT | `src/sci-pic/src/draw_pic.cpp`, `canvas.cpp:62-65`; tests `draw_pic_test.cpp:46-158` | CI (synthetic) |
| R-19 | Path 1 is bit-exact with the reference ScummVM for every picture (all variants the game draws), view and font of the tested release. | HYP high — `tools/graphics-compare` is the M2 check and the report claims it passes; not re-run | `tools/graphics-compare/run.sh` | local (claimed) |
| R-20 | Path 1 records a draw trace (which primitive owns each pixel, which pixels each line drew), following mirroring and overlays; the vector path is built only from this trace and the maps. | FACT | `src/sci-pic/include/sci/pic/draw_trace.hpp:43`; tests `draw_pic_test.cpp:175-215` | CI |
| R-21 | Views decode loops and cels (RLE runs); mirrored loops mirror rows and displacement; malformed views are rejected. | FACT | `src/sci-view/include/sci/view/view.hpp:38`; tests `tests/unit/view/view_test.cpp:49-70` | CI; fuzzed (`fuzz_view`) |
| R-22 | SCI0 cursors decode to black/white/transparent with the hotspot centred when byte 3 is set; resources of another size are refused. | FACT | `src/sci-view/include/sci/view/cursor.hpp:25`; tests `cursor_test.cpp:40-70` | CI |
| R-23 | Fonts decode glyph bitmaps MSB first; bitmaps that do not fit the resource are refused. | FACT | `src/sci-text/include/sci/text/font.hpp:31`; tests `tests/unit/text/font_test.cpp:25-39` | CI |
| R-24 | The 16 EGA colours are ScummVM's (`GfxPalette::setEGA`). | FACT | `src/sci-render/include/sci/render/colors.hpp:24-31` | CI (indirect, golden synthetic) |
| R-25 | Path 1 is what the game logic reads: priority and control maps come from path 1 only, never from the HD path. | FACT | VM paints via sci-pic (`src/sci-vm/src/gfx_paint.cpp:60-85`); compose reads HD priority only for drawing (`src/sci-render/src/compose.cpp:151`, `:207`) | CI (compose); local (scenario map comparison) |

## D. HD vector pipeline (path 2)

### D.1 Scene construction and gap rules

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-26 | A scene is built per plane (visual, priority) from path 1's maps and trace; every point of the 320x190 picture lies in exactly one face (a planar partition). | FACT | `src/sci-vector/src/build_scene.cpp:1262-1270`; test `tests/unit/vector/build_scene_test.cpp:217` | CI |
| R-27 | Topology (intersections, faces) is computed in exact rational arithmetic on doubled coordinates, bounded to ±1024. | FACT | `src/sci-vector/src/topology.hpp:23`; `src/sci-core/include/sci/core/exact.hpp:42`, `:127`; test `tests/unit/core/exact_test.cpp:11-47` | CI |
| R-28 | Gap rule: for each line end, lines whose 1x pixels touch within 2 pixels of the end are connected by extension, else join to the nearer end point, else a perpendicular connector, each at most 2 original pixels; connectors close the graph but are not drawn. Line ends in the outer row/column connect to the frame. | FACT | `build_scene.cpp:42-44` (`gap_limit = 4` doubled), `:134` (`contact_radius = 2`); tests `build_scene_test.cpp:98`, `:134`, `:142` | CI |
| R-29 | A real leak of the original fill stays as the player saw it: closing a gap never changes a face's value, because both sides take their value from the 1x map. | FACT | test `build_scene_test.cpp:115` (`leaks_stay_as_the_player_saw_them`) | CI |
| R-30 | Face values come from the 1x region pixels: uniform faces take the majority value outside the edge band; mixed faces and thin faces (no region pixel) fall back to 1x blocks. Lines with no visible pixel are left out. | FACT | `src/sci-vector/include/sci/vector/build_scene.hpp:60-71` (stats); tests `build_scene_test.cpp:86`, `:158` | CI |
| R-31 | Hint files are used only when their stored PIC SHA-256 matches the PIC data and their picture number matches; otherwise skipped with a log line. Invalid files are skipped with a warning. | FACT | `picture_hints.cpp:25-38`; `hints.hpp:104-107`; test `tests/unit/vector/hints_test.cpp:112` | CI |
| R-32 | Hint files are strictly validated (unknown members are errors; limits: 4096 items per array, 512 lines per chain, 200-char labels, 32-char ids, picture ≤ 999). | FACT | `src/sci-vector/src/hints.cpp:25-30`; `hints.hpp:100-104`; tests `hints_test.cpp:47-74`; `scripts/check-hints.py` | CI (unit, fuzz `fuzz_hints`, lint job) |
| R-33 | Chain hints force a curve kind (line, arc, spline, ellipse) on a chain of lines within 1 original pixel; a hint is dropped (and the chain re-fitted automatically) if no such curve keeps the faces or if the curve misses any 1x pixel of its lines. | FACT | `build_scene.cpp:46-48`, `:1126-1170`; tests `hint_scene_test.cpp:60-85`, `fitting_test.cpp:113-134` | CI |
| R-34 | Gap hints connect a line end to another line up to 8 original pixels; all are tried together, and if that adds mixed faces or 1x blocks they are tried one at a time and only harmless ones kept. | FACT | `build_scene.cpp:50-52`, `:866`, `:896`; test `hint_scene_test.cpp:93` | CI |
| R-35 | Every applied and rejected hint is reported with a reason (`SceneStats.applied_hints/rejected_hints`). | FACT | `build_scene.hpp:81-83`; `build_scene.cpp:743-745` | CI (indirect) |
| R-36 | Region, symmetry and relation hints are parsed and validated but **not used** by the analysis; relation ("in front of / behind / contains") hints are absent from all 86 hint files. | FACT | `grep -rn '\.relations\|\.regions\|\.symmetry' src tools` → no use outside `hints.cpp`; count over `hints/pic.*.json`: chains 1709, gaps 5, regions 508, symmetry 27, relations 0 | — (see V-02) |
| R-37 | The hint files are committed (`hints/pic.NNN.json`, 86 files) and frozen at milestone 9's state on `main`. | FACT | `ls hints` (86 `pic.*.json` + `README.md` + `schema.json`); STATUS.md decisions | CI (schema) / local (anchors, `sci-hint-tool check`) |

### D.2 Fitting, layers, stroke integrity

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-38 | Chains of short lines (each ≤ 12 original pixels in either axis) that turn gently (cosine ≥ 0.643) are replaced by a fitted straight line, circular arc, cubic spline or (closed chains) ellipse within 0.5 original pixel; a fit that would change the faces is rejected. | FACT | `build_scene.cpp:33-40`; `src/sci-vector/src/curves.cpp:19`; `fitting.cpp:21`; tests `fitting_test.cpp:53-103`, `build_scene_test.cpp:229` | CI |
| R-39 | Curves are flattened into chords of at most 0.25 original pixel. | FACT | `src/sci-vector/src/fitting.cpp:15-18` | CI (indirect) — see V-03 |
| R-40 | Visible lines become strokes 1 original pixel wide (radius 0.5, round ends), drawn as layers above the faces in drawing order. | FACT | `src/sci-vector/include/sci/vector/scene.hpp:66-71`; test `tests/unit/render/render_test.cpp:81` | CI |
| R-41 | Solid brushes become shapes (inscribed ellipse or box); textured brushes become 1x blocks. | FACT | test `build_scene_test.cpp:166` | CI |
| R-42 | Stroke integrity (addendum A1): line ends touching a contour are clipped at its near edge, corners between line ends are closed, a boundary drawn twice shows once in the colour seen, contacts the evidence does not support are left to command order. Can be compiled out (`SCI_WITHOUT_STROKE_INTEGRITY`). | FACT | `src/sci-vector/src/stroke_integrity.cpp:16-35`; `build_scene.cpp:1257-1261`; tests `stroke_integrity_test.cpp:67-118` | CI |
| R-43 | The A1 checks (overshoot, depth, doubled boundary, fragment, break) are measured by `check_integrity`, used only by `sci-hd-report` and tests, never at run time. | FACT | `src/sci-render/include/sci/render/integrity.hpp:1-30`; users: `tools/hd-report/main.cpp:238`, tests | CI (unit) / local (report) |
| R-44 | Depth/containment between regions is not modelled: layers follow the PIC command order only. | HYP medium — follows from R-36 (relations unused) and `build_scene.hpp:84-89` (stroke-integrity stats only, no depth stats); the code was not read in full for another depth mechanism | `build_scene.cpp` | — |

### D.3 Acceptance, rasterisation, priority map

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-45 | Acceptance per picture (visual scene reduced back to 1x vs path 1's pair map): ≥ 99 % agreement outside the edge band, no swap (4+ connected differing pixels), every pixel of a visible line touched by its stroke. Checked offline by `sci-hd-report`, not at run time. | FACT | `src/sci-render/include/sci/render/acceptance.hpp:1-18` (`passes()` at `:39`); `tools/hd-report/main.cpp:237` | local (`acceptance.cpp` in no test binary) |
| R-46 | HD pixels average 16 x 16 samples; each sample takes the topmost covering layer or its face's value. Colours are averaged in integers at 16 bits per channel and rounded with `((v*255)+32768)>>16`. | FACT | `src/sci-render/include/sci/render/rasterize.hpp:1-8`; `src/sci-render/src/rasterize.cpp:25`; `colors.hpp:43-66`; tests `render_test.cpp:56-81` | CI |
| R-47 | A dithered pair is shown as the plain average of its two sRGB colours (no gamma). | FACT | `colors.hpp:43-55`; test `render_test.cpp:56` (`pairs_blend_as_the_plain_average_of_srgb_values`) | CI |
| R-48 | Fallback faces are drawn as N x N blocks of the 1x values. | FACT | test `render_test.cpp:117` | CI |
| R-49 | The HD priority map is rendered from the priority scene: the value at each HD pixel's centre (no averaging). | FACT | `rasterize.hpp:66-70` | CI (indirect: compose tests) |
| R-50 | The picture area is 320N x 190N; N ranges 1..16 everywhere (config, composition). | FACT | `scene.hpp:18-19`; `src/app/platform/include/sci/platform/config.hpp:36`; `compose.hpp:94` | CI |
| R-51 | Rendering runs rows in parallel on the executor and the result does not depend on task order or thread count. | FACT | `rasterize.hpp:66`; tests `render_test.cpp:125`, `tests/threads/threads_test.cpp:106` | CI (incl. TSan) |

### D.4 Determinism, floating point, golden hashes

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-52 | Floating point: `double` only (no `float`/`long double` in project code), `-ffp-contract=off`, `-fno-fast-math`, `FLT_EVAL_METHOD == 0` at compile time. | FACT | `scripts/check-repo-policy.py` (`FLOAT_TYPES`); `cmake/SciCompilerFlags.cmake:33-34`; `src/sci-core/src/fp_profile.cpp:11-12` | CI (lint job, build) |
| R-53 | At run time, round-to-nearest, no FTZ, no DAZ and no fused multiply-add are checked on the main thread and on every worker thread; a failure stops the program. | FACT | `fp_profile.cpp:26-50`; `main.cpp:1196-1199`; `src/app/platform/src/thread_executor.cpp:24`; tests `tests/unit/core/fp_profile_test.cpp:9-16` | CI |
| R-54 | Library functions whose results differ between platforms are banned by clang-tidy: libm transcendental functions, `std::sort`/`partial_sort`/`nth_element` (stable sort with tie-breakers instead), `rand`/`srand`, `std::*_distribution`, `std::hash`. `std::sqrt` (correctly rounded) is allowed. | FACT | `.clang-tidy:79-84`; `scripts/run-clang-tidy.sh:12-13` | CI (lint job) |
| R-55 | Golden hashes of synthetic scenes and of a composed frame are identical on x86-64 and ARM64 (and Windows). | FACT | `tests/golden/synthetic_test.cpp:1-7`, `:73`, `:108` | CI |
| R-56 | Golden hashes of every picture background of the tested release (88 pictures, HD visual at N = 1, 2, 5, 10 and the original frame at N = 1) and of the 35 picture stacks the scripts draw, made with the repository's hints, must not change. | FACT | `tools/golden-hashes/main.cpp:1-25`, `:58`; `tests/golden/pictures.txt` (475 lines: 440 `pic`, 35 `stack`) | local |
| R-57 | Golden hashes do **not** cover the HD priority map, N = 3, 4, 6-9 or 11-16, views, fonts, cursors or composed game frames of the real game. | FACT | `tools/golden-hashes/main.cpp:58`, `:64-72` | — (see V-04) |
| R-58 | `analyser_version` (part of the cache key) must be bumped whenever `tests/golden/pictures.txt` changes. | FACT (rule stated in code, not enforced) | `build_scene.hpp:97-100`; no script checks it | untested |

## E. Analysis cache

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-59 | Scenes are cached on disk in `cache/` next to the executable; the key is SHA-256 over the game data (every resource), analyser version, scene format version, hint file bytes, picture, plane, mirroring, palette and overlays. | FACT | `src/app/platform/src/analysis_cache.cpp:50-78`, `:126-140`; `main.cpp:1125`; test `tests/unit/platform/analysis_cache_test.cpp:33` | CI |
| R-60 | Entries carry magic `SHDC`, version 1 and the key; an entry of another version or a stale key is refused (re-analysed). Writes go to `.tmp` and are renamed, so a cut-off write leaves no entry; write failures are logged, not fatal. | FACT | `analysis_cache.cpp:27-29`, `:90-103`, `:239-257`; test `analysis_cache_test.cpp:51` | CI |
| R-61 | The first launch analyses the plain variant (visual and priority) of every picture behind a progress bar; closing the window stops it. Mirrored, palette and overlay variants are analysed on demand during play. | FACT | `analysis_cache.cpp:289-330`; `main.cpp:406-458` | untested (progress UI); CI (cache logic) |
| R-62 | Cached scenes are read with a validating reader (damaged or inconsistent bytes are refused). | FACT | `src/sci-vector/include/sci/vector/scene_io.hpp:19-26`; tests `scene_io_test.cpp:73-96` | CI; fuzzed (`fuzz_scene`) |

## F. The VM (bytecode, kernel, determinism, patches, trace)

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-63 | The interpreter is a port of ScummVM's SCI0 engine at revision `ed949213…` (segments, scripts, bytecode, GC, kernel, graphics, sound, parser), translated to the project's style without exceptions. | FACT | file headers, e.g. `src/sci-vm/src/kernel.cpp:3-8`; `THIRD_PARTY.md` | local (scenarios, playthrough) |
| R-64 | The kernel table is ScummVM's SCI0-late DOS table (0x00-0x72, file functions after SetCursor, DoAvoider at 0x55) with ScummVM's signatures and workarounds; a signature mismatch without a workaround stops the game. | FACT | `kernel.cpp:59-62` (114 entries plus DoSound/Graph sub-tables); `src/sci-vm/src/machine.cpp:620-640` | local |
| R-65 | `kDoAvoider` is not implemented: calling it stops the game with a fault. | FACT | `src/sci-vm/src/k_stubs.cpp` (`not_implemented`) | untested |
| R-66 | The tested game never calls `kDoAvoider` on the paths played. | HYP medium — the 43-checkpoint playthrough (2,448,724 kernel calls) ended without a fault (MAP.md 6.4); other paths not known | — | local (playthrough) |
| R-67 | Where ScummVM would `error()`, the interpreter records the first fault and stops running scripts; the program shows "The interpreter stopped: …" and exits with 1. Warnings go to the `Vm` log category. | FACT | `src/sci-vm/src/faults.hpp:1-44`; `main.cpp:1162-1165` | CI (unit tests on synthetic machines) |
| R-68 | Bytecode execution, kernel calls, lists, strings, movement, animation, menus and controls have no CTest coverage beyond synthetic machines; they are verified only against the reference by local scenarios and the playthrough. | FACT | MAP.md 6.4 (`machine.cpp` 2.5 %, `gfx_menu.cpp`, `k_movement.cpp` 0 %) | local |
| R-69 | ScummVM's 16 script patches for Laura Bow 1 are applied when scripts load, all active by default (speed test, easter egg view, armor, Lillian bed, room 44 cursor, attic and left stairs lockups, chapel candles, Lilly/Gertie, copy-protection random fix, review notes dialog, obstacle collision). | FACT | `src/sci-vm/src/script_patches.cpp:612-742`; `script.cpp:73-74`; tests `tests/unit/vm/script_patcher_test.cpp:105-192` | CI (patcher, table consistency); local (effect) |
| R-70 | The copy protection is played, not bypassed (only ScummVM's "random fix" is applied). | HYP medium — the patch list has no bypass; the copy-protection patch only narrows kRandom's ranges (0-599, 0-999) so view 553's loop/cel do not overflow (`script_patches.cpp:554-556`); a `copy-protection.txt` scenario exists; no bypass was searched for elsewhere | `script_patches.cpp:719-721` | local |
| R-71 | Time is virtual: it passes only where the interpreter sleeps or waits (ScummVM's `EngineState::wait`, `speedThrottler`, `sleep`); game ticks are 1/60 s; a `kGetTime` elapsed read right after another lets 1 ms pass. | FACT | `src/sci-vm/src/timing.hpp:3-12`, `:45-112`; tests `tests/unit/vm/input_test.cpp:140-206` | CI |
| R-72 | Random numbers come from a port of ScummVM's `Common::RandomSource` with an explicit seed; the sequence matches the reference trace. | FACT | `src/sci-vm/src/k_math.cpp:41-59`; `timing.hpp:11`; test `input_test.cpp:231` | CI |
| R-73 | `sin`/`cos` of whole degrees come from a generated table of glibc x86-64 results (-360..360), other angles modulo 360; `kSqrt`, `kGetDistance`, `kSetJump` round to binary32 as ScummVM computes them. | FACT | `src/sci-vm/src/trigonometry.cpp` (table, `index()`); `src/sci-vm/src/binary32.hpp:10-15`; `k_math.cpp:66-68`, `:108`; `scripts/gen-trigonometry.py` | local — see V-05 |
| R-74 | Deterministic mode (`run_deterministic`, used by `sci-vm-trace`): seeded random source, replayed input only, deterministic date (Monday 1990-01-01 12:00:00 running with the virtual clock), stop at a tick or call count. | FACT | `src/sci-vm/include/sci/vm/interpreter.hpp:1-160`; `timing.hpp:63-66`; `src/sci-vm/src/interpreter.cpp:237`, `:266` | local (`interpreter.cpp` in no test binary) |
| R-75 | Kernel-call trace format: one line per call `<call> <tick> <name>(<args>) = <result>`, names as ScummVM's without `k`, numbers unsigned decimal, pointers `ssss:oooo`; plus actor, parser, layout, OPL, MIDI, screen and map files, all compared with the patched reference. | FACT | `interpreter.hpp:76-160`; `docs/formats/replay-and-trace.md:115-219`; `tools/scummvm-patches/0003-deterministic-trace.patch` | local |
| R-76 | 39 scenario runs (45 lines in `tests/scenarios/scenarios.txt`, 36 replay files) must match the reference in trace, screens, maps, actors, parser, layout, OPL and MIDI. | FACT (the check exists) / HYP high that it passes (claimed) | `tools/vm-trace/run-all-scenarios.sh`; MAP.md 6.3 | local (claimed) |
| R-77 | Replay files start with `sierra-hd replay 1` and hold `<tick> <kind> <data>` lines (key, text, move, down, up); text types each character then Enter; bad lines are rejected. | FACT | `src/sci-vm/src/events.cpp:184`; tests `input_test.cpp:33-69` | CI; fuzzed (`fuzz_replay`) |
| R-78 | The VM state hash: SHA-256 per component in fixed order (execution, segments, machine, clock, random, events, files, parser, sound, graphics), a total, and a "persistent" hash without execution; equal states hash equally at other addresses; hashing changes nothing. | FACT | `src/sci-vm/include/sci/vm/state_hash.hpp:1-32`; `src/sci-vm/src/state.cpp:320-756`; tests `tests/unit/vm/state_test.cpp:119-230` | CI |
| R-79 | Game-written files (kFOpen etc.) go to a host `FileStore` (`files/` next to the executable in the program), falling back to read-only game files; handles start at 1; ScummVM's line-reading rules. | FACT | `src/sci-vm/include/sci/vm/files.hpp:19`; `main.cpp:1137`; tests `tests/unit/vm/files_test.cpp:52-215` | CI (VM side); untested (`file_store.cpp`) |
| R-80 | The playthrough: the game is completable from start to end on the tested release, 43 checkpoints matching the reference, end tick 337,613. | FACT that the run reaches that tick in `sci-vm-trace` (re-run in stage 1a, MAP.md 6.4); HYP high that it matches the reference (claimed, not re-run) | `tests/playthrough/playthrough.txt`; `tools/playthrough/check.sh` | local |

## G. Animation and movement

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-81 | kAnimate, kAddToPic, kCanBeHere, kInitBresen/kDoBresen, kDirLoop, kBaseSetter, kSetJump behave as ScummVM's ports; each kAnimate cast (x, y, z, priority, view, loop, cel, signal) matches the reference. | FACT (ports) / local comparison | `src/sci-vm/src/gfx_animate.cpp`, `k_movement.cpp`, `gfx_compare.cpp`; actor files `interpreter.hpp:39-50`, `:96-99` | local |
| R-82 | The program draws sprites from kAnimate's cast at N (scaled N x N, occluded by the HD or 1x priority) rather than from the 1x screen. | FACT | `compose.hpp:1-15`, `:45-63`; tests `tests/unit/render/compose_test.cpp:70-181`, `tests/unit/vm/screen_test.cpp:120-164` | CI |
| R-83 | With `priority_snap = off` (default) sprites are occluded along the vector priority edges where the picture set the priority; with `on`, by the 1x priority in whole pixels; add-to-pic priority is always 1x blocks. | FACT | `src/sci-render/src/compose.cpp:151`, `:207`; tests `compose_test.cpp:70`, `:90`, `:129` | CI |

## H. Parser and Said

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-84 | The SCI0 parser uses vocabularies 0 (words), 900 (grammar branches), 901 (suffixes); broken vocabularies disable the parser (with warnings) instead of stopping the game. | FACT | `src/sci-vm/src/k_parse.cpp:51-54`, `:139-150`; test `tests/unit/text/parser_test.cpp:320` | CI |
| R-85 | Tokenising, suffix stemming, synonyms, numbers (not in early SCI0), pronouns referring to the last object, parse tree building and Said matching follow ScummVM's parser. | FACT | `src/sci-text/include/sci/text/parser.hpp:1-8`; tests `parser_test.cpp:167-302` | CI; fuzzed (`fuzz_parser`); local (parser files vs reference) |

## I. Windows, menus, text, cursor, interface at N

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-86 | Ports, windows, text, controls, menus, status line, cursor and transitions are ScummVM ports at 1x; their layout matches the reference (layout files). | FACT (ports) | `src/sci-vm/src/gfx_*.cpp`; `interpreter.hpp:106-110` | local |
| R-87 | At every N, every pixel the interface owns is the N x N block of its EGA colour, on top of sprites. | FACT | `compose.hpp:5-7`; tests `compose_test.cpp:116`, `tests/unit/platform/interface_check_test.cpp:121-125` (loops N = 1..16) | CI (synthetic); local (`check-interface.sh`, N = 1-16, game frames) |
| R-88 | The game's cursor is the resource scaled N x N (hotspot scaled); shown as the system cursor only when the system shows it at its own pixel size, else drawn over the frame; hidden when the game has none. | FACT | `src/app/src/game_screen.cpp:193-219`; `main.cpp:866-925`; tests `tests/unit/app/game_screen_test.cpp:202-225` | CI |
| R-89 | Where the game moves the mouse, the system pointer is warped to the centre of that 1x block; mouse positions map to 1x minus the letterbox, divided by N, with display scaling. | FACT | `game_screen.cpp:221-240`; `main.cpp:867-872`; tests `game_screen_test.cpp:236-269` | CI |
| R-90 | Kernel lines (kGraph DrawLine) over a picture are drawn as strokes in the HD background. | FACT | `src/app/platform/include/sci/platform/hd_pictures.hpp:3-9`, `:37-38`; test `screen_test.cpp:202` | CI (VM side) |
| R-91 | Only the changed 1x pixels are recomposed (16 x 16 tiles) and uploaded as dirty rectangles. | FACT | `src/app/platform/src/game_frame.cpp:34-60`; tests `tests/unit/platform/game_frame_test.cpp:98`, `compose_test.cpp:162` | CI |
| R-92 | When a new picture's HD images are not ready, the shown frame is held up to 1000 ms (room-entry budget), then the picture is shown at 1x until they come; the interpreter and music keep running. | FACT | `main.cpp:618`, `:826-838`; `game_frame.cpp:44-48`; `hd_pictures.hpp:44-51`; test `game_frame_test.cpp:127` | CI (frame logic); untested (`hd_pictures.cpp`) |

## J. Sound

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-93 | Music devices: AdLib (default) on Nuked OPL3 started as a dual OPL2, MT-32 on mt32emu with the user's ROMs and the game's `patch.001`, or off (silent driver). | FACT | `src/sci-audio/src/adlib.cpp:157`, `:549`; `main.cpp:997-1028`; tests `tests/unit/audio/adlib_test.cpp:115-296` | CI (AdLib); CI-skipped without ROMs (MT-32 synth) |
| R-94 | The AdLib driver ports ScummVM's MidiDriver_AdLib: 9 voices, no rhythm channel, patch banks of 48 or 96 instruments, voice stealing of the oldest, pitch wheel, hold pedal, volume/velocity/pan. | FACT | `src/sci-audio/include/sci/audio/adlib.hpp:92-99`; `adlib.cpp:58-61`; tests `adlib_test.cpp:152-296` | CI |
| R-95 | The MT-32 ROM folder: every `.rom` file ≤ 1 MiB is identified by content (mt32emu's size + SHA-1); the MT-32 1.07 control ROM is preferred, then other MT-32, then CM-32L, each paired with its PCM ROM; name order breaks ties. | FACT | `src/app/platform/include/sci/platform/mt32_roms.hpp:3-42`; `src/app/platform/src/mt32_roms.cpp:49-89`; tests `tests/unit/platform/mt32_roms_test.cpp:63-102` | CI |
| R-96 | `music = mt32` falls back to AdLib (with a warning) when ROMs are missing/wrong or `patch.001` cannot be read; no AdLib patches or no audio device → silent driver, game plays on. | FACT | `mt32_roms.hpp:45-47`; `main.cpp:1009-1027`; tests `mt32_roms_test.cpp:73-81` | CI (choice); untested (app) |
| R-97 | The music player is ScummVM's SciMusic/MidiParser_SCI: timer 250 calls/s on the virtual clock, priorities (lower waits, higher pauses), loops, channel filtering per device, signals/cues/tickers delivered on the same timer call as in ScummVM and independent of the device. | FACT | `src/sci-vm/src/music.cpp:169` (`Music::on_timer`), `:184` (`Music::send`); `adlib.hpp:96-97`; tests `tests/unit/vm/music_test.cpp:81-201`, `music_device_test.cpp:118-172` | CI |
| R-98 | Digital samples in sound resources are not played: a song with one stops the machine (fault). | FACT | `src/sci-vm/src/music.cpp:72-86`; test `music_test.cpp:193` (`digital_samples_stop_the_machine`) | CI |
| R-99 | The OPL output of every sound (first 20 s, AdLib) has a golden hash (119 sounds); `rand()` seeded with 1 so mt32emu's jitter repeats. | FACT | `tools/sound-render/main.cpp:17-29`, `:94`; `tests/golden/sounds.txt`; `main.cpp:1190-1194` | local; CI for a single OPL note (`opl3_test.cpp:142`) and MT-32 repeatability (`mt32_test.cpp:291`, needs ROMs) |
| R-100 | Audio output: synthesis and mixing on the main thread, handed to SDL's audio thread via an `SDL_AudioStream`; queue target 60 ms, resampling speed adjusted by up to 0.2 %; above twice the target new audio is dropped until back at target; after a real underrun (≥ a quarter of the target) the queue is topped up with silence. | FACT | `src/app/src/audio_output.hpp:7-28`, `:55`; `src/app/src/audio_output.cpp:150-197`; `src/sci-audio/src/mixer.cpp:73`; tests `tests/unit/app/audio_output_test.cpp:46-71`, `tests/unit/audio/mixer_test.cpp:80-172` | CI (with `SCI_BUILD_APP`) |
| R-101 | No audible gaps in normal play (the music timer runs during host waits; underruns are logged at exit). | HYP medium — the mechanism is R-100 and `timing.hpp` `run_music_in_wait`; the claim rests on manual listening and `tools/audio-gaps/find_gaps.py` captures, not on an automated check | `audio_output.cpp:113-116` | local / manual |
| R-102 | The volume (0-100 %) scales the output in the mixer. | FACT | `main.cpp:1012`, `:1026-1027`; test `mixer_test.cpp:126` | CI |

## K. Saves

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-103 | Saves are the project's own format: magic `SCIHDSAV`, format version 1, game identity (SHA-256 over the scripts and vocabularies), description, date, ticks, program version, the persistent VM state, SHA-256 checksum of the whole file. Not ScummVM-compatible (milestone 11 declined). | FACT | `src/sci-vm/src/save_game.hpp:30-31`; `save_game.cpp:1-7`, `:46-90` | CI |
| R-104 | Refusals with a message: not a save, another format version (naming both), truncated, wrong checksum, another release of the game, state not fitting the file, trailing bytes, inconsistent content. | FACT | `save_game.cpp:101-147`, `:179-197`; tests `tests/unit/vm/save_test.cpp:122-198` | CI; fuzzed (`fuzz_save`) |
| R-105 | A failed restore before the state is touched leaves the game running (the game reports the failure); a failure after it has started replacing state stops the game with a fault. | FACT | `src/sci-vm/src/k_files.cpp:300-321`; tests `save_kernel_test.cpp:332`, `:372` | CI |
| R-106 | Restore brings back the saved state including clock and random state, but the host's virtual time goes on, the play time is set back, the host master volume is kept, the global reverb comes from the save. | FACT | `k_files.cpp:313-320`; `timing.hpp:112-121`; tests `save_kernel_test.cpp:181-300` | CI |
| R-107 | The game's own save/restore dialogs are used (ScummVM's dialogs are not); at most 20 saves are listed, newest first then by slot; slots 1-99; new saves take free slots, the same id overwrites. | FACT | `k_files.cpp:55`, `:236`, `:291`, `:349`; `files.hpp:57-58`; tests `save_kernel_test.cpp:130-170` | CI |
| R-108 | Save files are `sierra-hd-001.sav` … `-099.sav` in `saves/` next to the configuration (or the configured folder, relative to it); other files are ignored. | FACT | `src/app/platform/src/save_folder.cpp:16-41`; `config.cpp:192-197`; tests `save_folder_test.cpp:54-133` | CI |
| R-109 | Round trip: saving and restoring at any point gives the same persistent state hash, and the run continues identically. | FACT (unit, synthetic) / HYP high for the game (claimed) | tests `save_test.cpp:64-82`; `tools/vm-trace/run-round-trips.sh` (5 ticks per scenario) | CI (synthetic); local (claimed) |

## L. The program

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-110 | Command line: `[--viewer] [folder]`, `[--replay f] [--seed n] [--fast] [--end-state f] [folder]`, `--version`, `--help`; `--viewer` with any game option is invalid; invalid usage prints the help and exits 2, `--help` exits 0. | FACT | `main.cpp:73-100`, `:958-992`, `:1177-1187` | untested |
| R-111 | Modes: HD (default) and original (every 1x pixel an EGA block); F12 toggles in the game, H in the viewer. | FACT | `config.cpp:77-81`; `main.cpp:756-759`; `src/sci-render/include/sci/render/original.hpp:17`; tests `compose_test.cpp:149`, `render_test.cpp:135` | CI (rendering); untested (keys) |
| R-112 | Scale: always an integer N; `auto` = the largest N that fits the window (capped at 16), a configured N is reduced to what fits; black bars around the frame. The window opens at the configured N as far as the display allows, at 3 with `auto`. | FACT | `main.cpp:734-739`, `:1057-1071` | untested |
| R-113 | Configuration `sierra-hd.ini` next to the executable: `[sierra-hd]` section, `key = value`, `;`/`#` comments (not in path values); required `format = 1`; keys `game_folder`, `mode` (hd/original), `scale` (auto/1-16), `priority_snap` (on/off), `mt32_rom`, `music` (adlib/mt32/off), `volume` (0-100), `saves`. Invalid values, a missing or other format, or a line without `=` are errors naming the line; unknown keys are warnings. A missing file means defaults. | FACT | `src/app/platform/src/config.cpp:16-145`; `config.hpp:35-56`; `main.cpp:326-340`; tests `platform_test.cpp:15-69` | CI |
| R-114 | The configuration is rewritten on every normal exit with all keys and their comments: unknown keys and the user's comments are dropped; a folder given on the command line, the F12 mode and the viewer's scale are persisted; a write failure is not reported. | FACT | `config.cpp:163-190`; `main.cpp:1093`, `:1133-1134`, `:1171` | CI (round trip `platform_test.cpp:50`); untested (persistence) |
| R-115 | `--replay`: the replay's input is scheduled into the game with seed `--seed` (default 0), and the date/time is the deterministic one; keyboard and mouse still work. Without `--replay` the seed is the performance counter (or `--seed`). | FACT | `main.cpp:1143-1161` | local (`tools/playthrough/program-run.sh`) |
| R-116 | `--fast`: waits do not wait for real time; the game sees the same virtual time and music timer calls; the screen is presented at most every 16 ms, vsync off, progress logged every 10 game minutes, audio beyond the queue dropped. | FACT | `main.cpp:687-728`; `audio_output.cpp:159-162` | local |
| R-117 | `--end-state`: on exit, writes `0 <tick> <globals…>` and the state hashes in `sci-vm-trace`'s formats so the program's playthrough can be compared with the tool's. | FACT | `main.cpp:1030-1055`, `:1166-1169` | local (`program-run.sh`) |
| R-118 | Real-time pacing: virtual time follows real time; if more than 250 ms behind, it catches up without a burst; the host presents frames and polls input during waits. | FACT | `main.cpp:654-685` | untested |
| R-119 | Fullscreen toggles with F11 or Alt+Enter (game and viewer); the window is resizable and high-DPI aware. | FACT | `main.cpp:470-473`, `:753-755`, `:1105-1108` | untested |
| R-120 | Keys reach the game as SCI events: ASCII or IBM scan codes, F1-F10 (with Shift), Ctrl+letter, Alt+key, keypad by Num Lock; right mouse button = Shift, middle = Ctrl, as the DOS mouse driver. | FACT | `game_screen.cpp:60-190`; `main.cpp:509-608`, `:771-792`; tests `game_screen_test.cpp:63-135` | CI (conversion); untested (SDL side) |
| R-121 | Logging: one facility with a sink installed by the program (stderr, `[category:level]`); `General` always on; `Resources` and `Vm` are off by default and the program has no option to enable them. | FACT | `src/sci-core/include/sci/core/log.hpp:3-7`, `:24-26`; `main.cpp:103-105`, `:1189`; `grep set_log_category_enabled src/app` → none | CI (`log_test.cpp:30`) |
| R-122 | Errors: user-facing problems are shown in a message box and on stderr; `SCI_ASSERT` stays on in release builds and aborts after logging; library code reports input problems as `Error` values (`std::expected`), built without exceptions. | FACT | `main.cpp:116-120`; `src/sci-core/include/sci/core/assert.hpp:3-15`; `cmake/SciCompilerFlags.cmake:36-45` | CI (error types) |
| R-123 | The window title shows the program version and the identified release. | FACT | `main.cpp:1123` | untested |
| R-124 | Without `hints/` next to the executable, pictures are analysed without hints (warning). | FACT | `main.cpp:393-401` | untested |
| R-125 | The program writes `sierra-hd.ini`, `cache/`, `files/` and `saves/` next to the executable. | FACT | `main.cpp:1086-1087`, `:1125`, `:1137`; `config.cpp:192-197` | untested — see V-08 |

## M. Room viewer

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-126 | `--viewer` browses pictures (HD/original, visual/priority map), views (loops, animation) and fonts, at N chosen with +/- (0 = largest that fits); Tab switches browser, Escape quits; HD backgrounds from the analysis cache. | FACT | `main.cpp:92-101`, `:156-190`, `:455-505`; `src/app/src/viewer.hpp:3-7`, `:43-95` | untested (`viewer.cpp` in no test binary) |

## N. Tools

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-127 | Tools are never linked into the program; they share `tools/common` (links sci-platform). | FACT | `tools/CMakeLists.txt`; MAP.md 2.2 | n/a |
| R-128 | `sci-hd-report` produces the replacement report (hints applied/rejected, fallbacks, acceptance, A1 integrity) per picture. | FACT | `tools/hd-report/main.cpp:237-238` | local |
| R-129 | `sci-hint-tool` proposes/checks/builds hint files; anchors (that hinted commands draw lines) are checked only with the game. | FACT | `scripts/check-hints.py:1-15`; `tools/hint-tool/main.cpp` | local (anchors); CI (schema) |
| R-130 | `sci-golden-hashes` writes/checks picture golden hashes, optionally through the analysis cache (same hashes required). | FACT | `tools/golden-hashes/main.cpp:1-25` | local |
| R-131 | `sci-vm-trace`, `sci-sound-render`, `sci-hd-frames`, `resource-compare`, `graphics-compare`, `playthrough/*`, `reference-scummvm/build.sh` (refuses another ScummVM revision) support the local checks of MAP.md 6.3. | FACT | MAP.md 2.2; `tools/reference-scummvm/build.sh:23-30` | local |

## O. Build, CI and quality rules enforced by scripts

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-132 | Upstream Clang 23.1.4 only (clang-cl on Windows), C++23 without extensions, CMake ≥ 3.28 + Ninja, toolchain container pinned by digest and SHA-256. | FACT | `cmake/SciCompilerFlags.cmake:8-12`; `cmake/SciTargets.cmake:17-18`; `docker/Dockerfile` | CI |
| R-133 | Warnings `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast …` are errors on project code (`SCI_WARNINGS_AS_ERRORS` ON); vendored code is not held to them. | FACT | `cmake/SciTargets.cmake:20-39`; `cmake/SciThirdParty.cmake:64` | CI |
| R-134 | clang-tidy with the configured check set and `WarningsAsErrors: '*'`, cognitive complexity ≤ 25, naming rules; every `.cpp` outside `third_party/`. 137 `NOLINT` suppressions exist (each should state a reason). | FACT | `.clang-tidy:20-84`; `scripts/run-clang-tidy.sh:12-13`; `grep -c NOLINT` | CI (lint job) |
| R-135 | Repository policy: no tracked game resources, ROMs or images (none allowed); no `float`/`long double` in `.cpp`/`.hpp`; SPDX header in the first three lines of `.cpp`/`.hpp`. | FACT | `scripts/check-repo-policy.py:1-60` | CI (lint job) — see V-06 |
| R-136 | clang-format on every source. | FACT | `scripts/check-format.sh`; `ci.yml:148-171` | CI |
| R-137 | Sanitizers: ASan+UBSan (`-fno-sanitize-recover=all`) and TSan presets run all tests in the gate; draft PRs run ASan, then TSan on the `threads` label only; clang-cl: ASan only. `-fwrapv` only in Release/RelWithDebInfo without sanitizers. | FACT | `cmake/SciCompilerFlags.cmake:53-96`; `.github/workflows/ci.yml:179-267` | CI |
| R-138 | Fuzzing: 12 libFuzzer targets, 60 s each in the gate with a cached corpus, 30 s in pre-push; their seed corpora replay as CTest tests in every preset. | FACT | `tests/fuzz/CMakeLists.txt`; `ci.yml:277-298`; `cmake/SciTesting.cmake:31-37` | CI |
| R-139 | CI runs only on pull requests to `main` (not docs-only) and by manual dispatch; pushes run nothing; actions pinned by SHA. | FACT | `ci.yml:1-135` | n/a |
| R-140 | The local pre-push gate: format, policy, hints, clang-tidy, release, asan, tsan, fuzz 30 s, cross-aarch64 under qemu-user; game-file checks are separate. | FACT | `scripts/pre-push.sh:9-53` | local |
| R-141 | Release builds on `v*` tags for Linux x64/arm64, Windows x64/arm64, macOS arm64, packaged with `hints/`, `USER-README.md` as `README.md` and the licences. | FACT | `.github/workflows/release.yml`; `scripts/package-release.sh:15-29` | CI (release workflow) |
| R-142 | The program links statically (`-static-libstdc++ -static-libgcc` on Linux unless sanitized), with SDL3, Nuked OPL3 and mt32emu vendored and linked statically. | FACT | `src/app/CMakeLists.txt:10-21`; `cmake/SciThirdParty.cmake` | CI — see V-09 |

## P. Platforms

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-143 | Linux x64 and arm64 are the first priority, Windows x64 (and arm64 release) second; both are in the CI gate. macOS is optional, built only by manual dispatch and on release. | FACT | `ci.yml:244-267`, `:315-363`, `:376-403`; `docs/ASSUMPTIONS.md:487-490` | CI (Linux, Windows); manual (macOS) |
| R-144 | The program on x86-64 needs SSE2 double evaluation (no x87); ARM64 results equal x86-64's. | FACT | `fp_profile.cpp:9-12`; R-55 | CI |

---

## V. Observations: contradictions and gaps between code and docs

No fixes or recommendations here (brief: stage 1b). Severities are
provisional, for stage 2.

- **V-01 (5.25" release and hints; Medium).** `identify.cpp:131-132` and
  `docs/USER-README.md:31-33` say the 5.25" release's pictures are drawn
  without the HD hints. Nothing in the code acts on the release (R-06):
  hints are skipped only if the 5.25" PIC data differs from the 3.5"'s.
  Whether it does is **not verified** (no 5.25" copy available; same
  version number 1.000.046 suggests the PIC data may be identical, in which
  case the hints *are* used and the warning is false). Also unverified:
  that the 5.25" release runs at all (no test, no scenario).
- **V-02 (relation hints; Medium).** `src/sci-vector/include/sci/vector/hints.hpp:78-80`
  says relation hints are "Used by the vector path from milestone 3b";
  addendum A1 (`docs/SPEC_ADDENDA.md:25-31`, `:103`) asks for depth and
  containment relations from the hints. No code reads them and no hint
  file has any (R-36, R-44). Region and symmetry hints are informational
  only (documented as such in `hints.hpp`).
- **V-03 (comment vs range of N; Low).** `fitting.cpp:15-17` justifies
  `max_chord = 0.25` with "the largest scale, N = 10" while N goes to 16
  (`config.hpp:36`, `compose.hpp:94`). At N = 16 a chord is 4 screen
  pixels; the flattening error stated there was not recomputed.
- **V-04 (golden coverage; Medium).** The picture golden hashes cover HD
  visual at N = 1, 2, 5, 10 and original at N = 1 only (R-56, R-57): no
  HD priority map (which decides sprite occlusion), no N > 10, no views,
  fonts, cursors, composed frames. All game-dependent goldens are local
  only.
- **V-05 (trigonometry comment; Low).** `src/sci-vm/src/trigonometry.hpp:3-7`
  says angles outside -360..360 "use the C library"; `trigonometry.cpp`
  uses the angle modulo 360 from the table and notes it "may differ from
  glibc's in the last bits", i.e. from the reference ScummVM for such
  angles. Whether the game ever passes such angles is not known.
- **V-06 (repository policy misses SCI0 patch files; Medium).**
  `scripts/check-repo-policy.py` says no game resources are tracked, but
  its pattern (`resource.map|resource.NNN|message.*|*.scr|*.hep|*.drv|*.p56|*.v56|*.sci|*.rom`)
  does not match SCI0 patch files named `TYPE.NNN` (e.g. `SCRIPT.052`,
  `TEXT.052`, `PATCH.001`, `VIEW.123`), which are game data. Nothing such
  is tracked today (`git ls-files | grep -iE '^(script|text|patch|view|pic|font|sound|vocab|cursor)\.[0-9]{3}$'`
  finds nothing) [checked only by that grep].
- **V-07 (deterministic-mode doc vs program; Low).**
  `docs/formats/replay-and-trace.md:40` says host input is ignored in the
  deterministic mode; the program's `--replay` keeps keyboard and mouse
  working (`main.cpp:80`, R-115). The doc describes `sci-vm-trace`; the
  program's replay mode is not documented as differing.
- **V-08 (writes next to the executable; Medium, HYP).** The program writes
  its configuration, cache, game files and saves next to the executable
  (R-125) and does not report a failed configuration write (R-114).
  HYPOTHESIS (medium confidence): in a read-only install location (e.g.
  `Program Files`, a system package) the configuration and saves cannot be
  written; the cache write failure is only logged. Not tested on any
  platform.
- **V-09 (static linking of LGPL code; Low, HYP).** Nuked OPL3 and mt32emu
  are LGPL-2.1+ and linked statically into a GPL-3.0+ program. GPL-3.0
  covers the whole, so this is probably compliant (HYPOTHESIS, low
  confidence; not a legal opinion; out of the code's scope).
- **V-10 (log categories; Low).** `Resources` and `Vm` warnings (script
  bugs worked around, skipped resources) can never be seen in the program
  (R-121); only tools enable them.
- **V-11 (ini comment; Low).** The rewritten configuration says
  `saves` "empty: saves next to this file" (`config.cpp:184`); the code
  uses a `saves/` subfolder next to it (`config.cpp:194`). USER-README is
  correct.
- **V-12 (analyser version rule; Low).** The rule "bump `analyser_version`
  whenever `tests/golden/pictures.txt` changes" (`build_scene.hpp:97-99`)
  is not checked by any script. History: since the cache was added
  (`63c7fb8`) the golden file only gained lines (`9ceebbd`, stacks), so the
  rule has held so far.
- **V-13 (MAP.md correction).** MAP.md 3.3 says 87 hint files; there are
  86 `pic.NNN.json` (plus `README.md` and `schema.json`).
- **V-14 (5.25" and other releases untested; Low).** R-04: accepted, but
  no scenario, golden or unit test covers it.
- **V-15 (docs that describe intent, not code).** Carried from MAP.md 8:
  `docs/ARCHITECTURE.md` stale (milestone 4a), `src/sci-vm/README.md`
  stale (silent music, saves failing). Not repeated here.

## W. Counts

Computed from the tables (script over the `| R-` rows):

- R-items: **144** (R-01 … R-144).
- Kind: FACT **135**; HYPOTHESIS **6** (R-15, R-19, R-44, R-66, R-70,
  R-101); mixed **3** (R-76, R-80, R-109: the check exists = FACT; that it
  passes on the game = HYPOTHESIS high, claimed by the milestone reports,
  not re-run).
- Tested: CI **95**, local only **27**, untested **15**, none / not
  applicable **7** (R-06, R-07, R-36, R-44, R-57, R-127, R-139).
  Counting rule: "CI" if any CTest test checks the item, even when its
  behaviour on the real game is only checked locally (many CI items are
  "CI (synthetic); local"). So "CI" overstates what CI protects for the VM:
  see MAP.md 6.4 (sci-vm 38 % lines in CI).
- Observations: 15 (V-01 … V-15).
