# Stage 2: gaps between the code and the original requirements

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-05.
Read-only: nothing outside `review/` was changed.

**Original requirements compared:** the customer's spec "Task Spec: HD Port
of The Colonel's Bequest (SCI0)", version 15 (not in the repository, quoted
by section, e.g. "§6.4"), the addenda in `docs/SPEC_ADDENDA.md` (A1; A2
withdrawn; A4; there is no A3), and the customer's decisions in
`docs/ASSUMPTIONS.md`. The code side is `review/REQUIREMENTS_RECOVERED.md`
(R-xx, V-xx) and `review/MAP.md`.

## How to read this file

**Classes** (the brief's five, with the customer's rules applied):

| Code | Class |
|---|---|
| FULL | fully implemented |
| PART | partially implemented or deviating (an approved deviation is still PART, with its approval named) |
| DROP | not implemented — **dropped by the customer (answer 4 after stage 1a, "If it is not implemented, then drop it"; ASSUMED reading, to be confirmed at stage 3)**. Not work; severity none. |
| DECL | not implemented — **declined by the customer** (milestones 10/A4, 11, 12, exact area coverage; the waived manual playthrough). Severity none. |
| CREEP | implemented but not required (scope creep) |
| AMB | contradiction or ambiguity in the requirements themselves |

Two customer rules give a class a fixed severity: 5.25" release acceptance
is **untested scope** (answer 2 after stage 1b: FULL as code, severity
none), and the A1 relation hints are **DROP** (answer 3 after stage 1b,
confirmed).

**Severity** (brief): Critical — broken core functionality, data loss, or
blocks iteration 2; High — significant deviation from requirements or
serious risk; Medium — noticeable quality or maintainability issue; Low —
cosmetic or minor. FULL items carry "—"; DROP, DECL and untested scope
carry "none".

**Evidence.** "Code" gives `file:line` on `main` (paths from the repository
root; `main.cpp` is `src/app/src/main.cpp`) and the test where one exists.
"local (claimed)" means a game-dependent check the milestone reports say
passes, not re-run for this audit. Statements marked **[not verified]**
are not facts; the reason is given. Every item with a severity says why.

Granularity: one G-item per requirement of the spec as written (a
sentence or a short group of sentences that are checked together). Where
one defect touches several sections (macOS, the release), it is one item
with all references, so it is counted once.

---

## §1 Purpose and scope

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-01 | §1.1 HD vector backgrounds; sprites original, scaled by an integer factor with no added detail | R-26…R-51, R-82 | FULL | — | `src/sci-render/include/sci/render/compose.hpp:1-15`; `tests/unit/render/compose_test.cpp:70-181` | Sprites are original cels in N x N blocks (A2 undithering withdrawn; no undithering code in `src/`). |
| G-02 | §1.1 "completable from start to finish, with sound" | R-80, R-93, R-99 | FULL | — | `tests/playthrough/playthrough.txt`; `tools/playthrough/check.sh`, `program-run.sh`; `reports/milestone-9.md:80-100` | local. Stage 1a re-ran the playthrough to its end tick 337,613; the match with the reference is claimed (43 checkpoints), not re-run. The customer's manual playthrough is G-118. |
| G-03 | §1.1, §6.1 "Game logic always behaves as in the original" | R-25, R-63, R-76 | FULL | — | `src/sci-vm/src/gfx_paint.cpp:60-85` (logic reads path 1); `tools/vm-trace/run-all-scenarios.sh` | local (claimed). CI cannot detect a VM regression (MAP.md 8.1, sci-vm 38 % lines in CI): not a spec gap (§11 puts game checks local), but a High risk for stage 5, carried as a finding. See also G-104 (script patches change game logic). |
| G-04 | §1.2 Game: the English DOS EGA version; tested release accepted | R-01, R-02, R-05 | FULL | — | `src/app/platform/src/identify.cpp:25-51`, `:97-105`; `tests/unit/platform/platform_test.cpp:71-79` | Positive identification needs the game (local). |
| G-05 | §1.2 Platforms: Linux x86-64 and ARM64 | R-143, R-144 | FULL | — | `.github/workflows/ci.yml:179-267`; `scripts/pre-push.sh:9-53` (qemu-user) | Gate platform, first priority (customer, 2026-10-22). |
| G-06 | §1.2 Platforms: Windows x86-64 and ARM64 | R-143 | FULL | — | `ci.yml:244-267`; `.github/workflows/release.yml` | x64 release + ASan in the gate, ARM64 release build. |
| G-07 | §1.2 macOS (x86-64 and ARM64) a supported platform; §3.2 macOS CI runner; §5.2 sanitizers on macOS; §10.4 and §14 CI on Linux, macOS and Windows, binaries for all three OSes | R-143 | PART | **High** | Build blockers recorded, not fixed: `std::jthread` in `src/app/platform/src/thread_executor.cpp:29`, `std::chrono::zoned_time` in `main.cpp:638` (`docs/ASSUMPTIONS.md:498-505`); macOS jobs only on manual dispatch (`ci.yml`, `release.yml:6-16`, `:110-129`); no macOS x86-64 job at all | The customer made macOS "optional" but kept it "a supported platform" (`docs/ASSUMPTIONS.md:487-511`). The program is known not to build there with the Apple SDK's libc++; no macOS sanitizer run; no macOS x86-64 target in any workflow. `README.md:36-40` gives macOS build steps as if they worked. Significant deviation from an in-scope platform; whether answer 4 drops it is a question (stage 3). |
| G-08 | §1.2, §6 HD mode and original mode, switchable at any time | R-111 | FULL | — | `main.cpp:756-759` (F12), `src/app/platform/src/config.cpp:77-81`; `tests/unit/render/compose_test.cpp:149` | The key handling itself is untested (app layer). |
| G-09 | §1.2, §8 Sound: AdLib and MT-32 (user ROMs only) | R-93, R-95 | FULL | — | `src/sci-audio/src/adlib.cpp:157`; `main.cpp:997-1028` | |
| G-10 | §1.2, §9 Saves in the project's own format | R-103 | FULL | — | `src/sci-vm/src/save_game.hpp:30-31` | |
| G-11 | §1.3 Out of scope: other games/versions, localisation, other sound devices and GM, gamepad, key remapping, screenshot/video capture, interpolation and effects, gradients, output dithering, 10-bit, installers, browser version | — | FULL | — | `grep` of `src/` for these: none in the program | No out-of-scope feature in the program. Tools write PNG images for reports (§3.1 asks for that; not a capture feature of the product). |

## §2 Fixed decisions

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-12 | C++23, upstream LLVM Clang everywhere (clang-cl on Windows), CMake | R-132 | FULL | — | `cmake/SciCompilerFlags.cmake:8-12`; `cmake/SciTargets.cmake:17-18` | Clang 23.1.4; other compilers refused at configure time. |
| G-13 | Own interpreter, not a ScummVM fork; SCI0 logic ported from ScummVM | R-63 | FULL | — | `src/sci-vm/src/kernel.cpp:3-8`; `THIRD_PARTY.md` | |
| G-14 | Licence GPL v3 or later; every other component compatible | — | FULL | — | `LICENSE`; `THIRD_PARTY.md` | V-09: LGPL-2.1+ libraries linked statically into a GPL-3.0+ program, probably compliant **[not verified; not a legal opinion]**. |
| G-15 | Square pixels, 320N x 200N frame, integer N | R-50, R-112 | FULL | — | `main.cpp:734-739`, `:1057-1071` | |
| G-16 | Rendering deterministic, bit-identical on every supported machine | R-52…R-55 | FULL | — | `tests/golden/synthetic_test.cpp:73`, `:108` | CI checks synthetic scenes on x86-64, ARM64 and Windows; the real-game goldens are local (G-46). macOS never checked (G-07). |
| G-17 | SDL3, Nuked OPL3, mt32emu vendored at pinned versions; further dependencies justified in docs/; fewer preferred | — | FULL | — | `third_party/` (exactly these three); `THIRD_PARTY.md`; `docs/ASSUMPTIONS.md:862-866`, `:2022-2033` | No further dependency. The recorded vendored checksums are checked by no script (MAP.md 8.11) — not required by the spec. |
| G-18 | Game resources and ROM files are never committed | R-135 | PART | Medium | `.gitignore` (resource, driver, ROM patterns); `scripts/check-repo-policy.py:1-60` | Holds today: a name check over all local history (`git log --all --name-only`) found no game file and no game-derived image (only two SDL documentation PNGs) **[checked by file name only, not by content]**. But neither `.gitignore` nor the policy script matches SCI0 patch files `TYPE.NNN` (`SCRIPT.052`, `TEXT.052`, `PATCH.001`, …), which the tested release ships (V-06): one careless `git add` of a game folder copy would not be caught. |
| G-19 | Natural language English in code, comments, commits, docs | — | FULL | — | sampled | |

## §3 Environment and inputs

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-20 | §3.1, §3.2 Pinned toolchain container; ARM64 locally with qemu-user and a cross toolchain | R-132, R-140 | FULL | — | `docker/Dockerfile`; `docker/image-tag.sh`; `scripts/pre-push.sh` (cross-aarch64) | |
| G-21 | §3.2 CI runners for Linux x86-64/ARM64, macOS, Windows (x86-64, ARM64) configured from the start; runner availability and cost checked at milestone 0 | R-139 | FULL | — | `docs/ASSUMPTIONS.md:226-283` | All five runner kinds ran at milestone 0. macOS later made optional: see G-07. |
| G-22 | §3.3 ScummVM pinned revision recorded; reference ScummVM built (no scaler, aspect correction, undithering), patched for dumps, traces, deterministic mode; patches in `tools/scummvm-patches/`; reference tooling in its own build target, never linked into the product | R-127, R-131 | FULL | — | `tools/reference-scummvm/REVISION`, `build.sh:23-30`; `tools/scummvm-patches/0001…0003`; `tools/CMakeLists.txt` | Revision `ed949213…` (`docs/ASSUMPTIONS.md:32-36`). |
| G-23 | §3.3 Record every assumption and decision in `docs/ASSUMPTIONS.md`; say when something is missing | — | FULL | — | `docs/ASSUMPTIONS.md` (2,895 lines) | Some entries are out of date (e.g. `:28` "private", see G-24). |
| G-24 | §3.4 "The repository is private for now and is cleaned before any publication (section 14)"; §14 "Before any publication: a cleanup pass over the whole history … and the licensing review of the hint files (section 15)"; §15 hint-file licensing (labels and geometry only, no image data; terms of the model used) | — | PART | **High** | `gh api repos/CityRunner/sierra-hd` → `public`; made public on 2026-10-22 (`reports/milestone-4b.md:218-221`, `docs/ASSUMPTIONS.md:510-511`) | The repository has been public for two weeks. No record of the history cleanup pass or of the hint-file licensing review was found (`grep` of `docs/`, `reports/`, `hints/README.md`). The hint format itself keeps to labels and integers (`hints/schema.json`), and the name check of G-18 found no game data — but the review the spec requires before publication is not documented. Publication was the customer's decision at the 4b gate (to save CI minutes); whether the two §14 steps were knowingly waived is not recorded. Serious legal/licensing risk → High. |
| G-25 | §3.4 Repository starts with LICENSE, a `.gitignore` for game resources and ROMs, and `docs/`, `tools/`, `tests/`, `third_party/`, `hints/`, `reports/` | — | FULL | — | repository root | |
| G-26 | §3.4 Third-party components in `third_party/` at a pinned commit or checksum, with their licences | — | FULL | — | `THIRD_PARTY.md`; `third_party/*/VENDORED.md` | |
| G-27 | §3.4 Images derived from the game art never committed; `reports/images/` git-ignored | — | FULL | — | `.gitignore` `/reports/images/`, `/reports/dumps/` | History name check as in G-18. |
| G-28 | §3.5 Sound checked by ear by the customer at milestone 7 if the machine has no audio | — | FULL | — | `reports/milestone-7.md:196-200` | Customer listened (2026-10-28). |

## §4 Architecture

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-29 | §4.1 The nine libraries with the listed responsibilities, plus app | — | PART | Low | MAP.md 1.1; `docs/ASSUMPTIONS.md:1162-1186` | Approved/recorded deviations: the music player (sound resource playback) and the save format live in sci-vm, not sci-audio / app; the window-free part of app is a library `sci-platform`. Reasoned (sound timing and saves are VM state) and documented; Low because §4.1 and the docs now disagree. |
| G-30 | §4.1 `tools/` (room viewer, resource dumper, golden-hash generator, reference tooling), `tests/` (unit, fuzz, golden) | R-127…R-131 | FULL | — | `tools/`, `tests/` | |
| G-31 | §4.2 One CMake target per library, narrow public interfaces, no cycles, no access to another library's internals | — | PART | Low | `cmake/SciTargets.cmake:42-63` | Cycles impossible by construction; `src/` private. But four declared dependency edges are unused (MAP.md 1.3, 8.7) and five test targets include libraries' private `src/` (MAP.md 1.4). |
| G-32 | §4.2 Only app (and the audio-output part of sci-audio) use window system, OS files, threads; parallel libraries use an executor interface supplied by app | R-51 | FULL | — | `src/sci-core/include/sci/core/executor.hpp`; `src/app/platform/src/thread_executor.cpp` | Audio output is in app, not sci-audio (allowed). |
| G-33 | §4.2 sci-core, sci-vector, sci-render have no external dependencies | — | FULL | — | `cmake/SciTargets.cmake:42-63` (`sci_add_library` takes none) | |
| G-34 | §4.3 VM and frame output on the main thread; audio on its own thread; analysis and tile rasterisation on worker threads | R-100 | PART | Low | `src/app/src/audio_output.cpp:150-197`; `docs/ASSUMPTIONS.md:1833-1846` | Synthesis and mixing run on the main thread; SDL's audio thread only plays the queue. Decided and reasoned (no shared state). |
| G-35 | §4.3 Results never depend on the number of threads or on scheduling | R-51 | FULL | — | `tests/unit/render/render_test.cpp:125`; `tests/threads/threads_test.cpp:106` (TSan) | |

## §5 Engineering rules

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-36 | §5.1 Committed clang-format and clang-tidy (with a cognitive-complexity limit) enforced in CI; exceptions suppressed in place with a stated reason | R-134, R-136 | PART | Low | `.clang-tidy:20-84`; `scripts/check-format.sh` | 20 of 137 `NOLINT` suppressions state no reason (`grep -E '// NOLINT(NEXTLINE)?(\([^)]*\))?\s*$'`), e.g. `main.cpp:113`, `:142`, `:909`. |
| G-37 | §5.1 Ported ScummVM code in project style; each ported unit carries the licence header and its source file and revision; temporary compatibility layer removed by milestone 9 | R-63 | FULL | — | e.g. `src/sci-vm/src/kernel.cpp:3-8` | Every `.cpp` in sci-resource, sci-pic, sci-text and sci-vm names its ScummVM source in its header except 14 files of the project's own design (e.g. `interpreter.cpp`, `mixer.cpp`) **[not checked one by one that those contain no ported code]**. No compatibility layer found. |
| G-38 | §5.1 Quirks and workarounds copied from the original/ScummVM are commented with why and where from | R-64, R-69 | FULL | — | `src/sci-vm/src/script_patches.cpp:612-742` | sampled. |
| G-39 | §5.1 Each library has a short README with purpose and invariants | — | PART | Low | `src/*/README.md` (10) | `src/app/platform` (sci-platform) has none; `src/sci-vm/README.md:7`, `:11` is stale (MAP.md 8.5). |
| G-40 | §5.1 One error-handling scheme chosen and documented at milestone 0 | R-122 | FULL | — | `docs/ASSUMPTIONS.md:172-192`; `cmake/SciCompilerFlags.cmake:36-45` | |
| G-41 | §5.1 One logging facility in sci-core with categories and an app-supplied sink; trace categories (kernel calls, sound signals) stay in release builds, off by default | R-121 | PART | Low | `src/sci-core/include/sci/core/log.hpp:3-26`; `main.cpp:103-105` | The program can never turn on a category other than `General` (V-10); kernel-call and sound traces exist only in `sci-vm-trace`'s deterministic mode, not as switchable categories of the release program. |
| G-42 | §5.1 Optimisations only after measurement; the simple reference implementation kept and used in differential tests | — | FULL | — | `src/sci-render/include/sci/render/reference.hpp`; `tests/unit/render/render_test.cpp:110-114` | |
| G-43 | §5.1 No dead code, no debug hacks outside build options, no TODO without a reference | — | PART | Low | `src/sci-audio/src/component.cpp`, `src/sci-vm/src/component.cpp` | Dead `component_name()` placeholders (MAP.md 8.8). No `TODO`/`FIXME` in `src/` or `tools/` (grep). Debug features are build options (`SCI_DEBUG_HIGHLIGHT`, `SCI_WITHOUT_STROKE_INTEGRITY`). |
| G-44 | §5.1, §14 `docs/ARCHITECTURE.md` describes modules, data flow, trade-offs and is updated at every milestone | — | PART | Medium | `docs/ARCHITECTURE.md:4-5` ("State: milestone 4a"), `:75`, `:23`, `:298`, `:307` | Not updated since 4a (MAP.md 8.4): five milestones of VM, sound, saves and composition are missing or described as planned. Medium: the main maintenance document is wrong for most of the code iteration 2 will touch. |
| G-45 | §5.2 Signed fixed-width integers by default; overflow is a bug; `-fwrapv` in release, not in sanitizer builds | R-137 | FULL | — | `cmake/SciCompilerFlags.cmake:91-96` | Debug builds also lack `-fwrapv` (MAP.md 8.12) — consistent with the spec. |
| G-46 | §5.2 No plain `char` for data; no unspecified order (stable sorts with tie-breakers); no platform-dependent std facilities in output code | R-54 | FULL | — | `.clang-tidy:79-84`; `scripts/run-clang-tidy.sh:12-13` | Plain `char` appears only for text (`src/sci-core/src/json.cpp`, `game_screen.cpp:60`) **[sampled, not exhaustive]**. |
| G-47 | §5.2 No uninitialised memory; no type punning except memcpy/bit_cast; explicit little-endian reads; RAII, no raw owning pointers | — | FULL | — | `src/sci-core/include/sci/core/byte_reader.hpp`; `src/sci-audio/src/mt32.cpp:136`, `audio_output.cpp:82` | The two `new` are wrapped into `unique_ptr` at once (private constructors). **[sampled]** |
| G-48 | §5.2 All file parsers bounds-checked and covered by libFuzzer targets | R-138 | PART | Medium | `tests/fuzz/*.cpp` (12 targets) | Fuzzed: map/volumes/patches, decompression, PIC, view/font/cursor, vocabulary + Said, JSON/hints, replay, save, cached scene, game folder. **Not fuzzed:** script resources (`src/sci-vm/src/script.cpp`), sound resources (`src/sci-vm/src/midi_parser.cpp`, `music.cpp`), the AdLib and MT-32 patch resources (`adlib.cpp`, `patch.001`), the configuration file (`config.cpp`). Scripts and songs are untrusted game input. |
| G-49 | §5.2 Warnings are errors; sanitizers UBSan/ASan/TSan on Linux and macOS, ASan on Windows | R-133, R-137 | FULL | — | `cmake/SciTargets.cmake:20-39`; `ci.yml:179-267` | Linux and Windows as required; macOS part counted in G-07. |
| G-50 | §5.3 Topology in exact integer arithmetic on 1x coordinates; intersections exact rationals in int64 | R-27 | FULL | — | `src/sci-vector/src/topology.hpp:23`; `src/sci-core/include/sci/core/exact.hpp:42`, `:127`; `tests/unit/core/exact_test.cpp:11-47` | Doubled coordinates, products bounded below 2^60. |
| G-51 | §5.3 Floating-point profile (double only, `-ffp-contract=off`, no fast-math, SSE2, no libm transcendentals in output code, no FTZ/DAZ asserted on every rendering and analysis thread) | R-52, R-53, R-144 | FULL | — | `src/sci-core/src/fp_profile.cpp:11-50`; `thread_executor.cpp:24`; `tests/unit/core/fp_profile_test.cpp:9-16` | The VM's sin/cos come from a table of glibc results (R-73) — game logic, not rendering; V-05 (comment) Low, counted in G-128. |
| G-52 | §5.3 Colours, blending, palette indices and hashes stay integer | R-46 | FULL | — | `src/sci-render/include/sci/render/colors.hpp:43-66` | |
| G-53 | §5.3 GPU only presents; all rasterisation on the CPU | R-91 | FULL | — | `src/app/platform/src/game_frame.cpp:34-60` | |
| G-54 | §5.3 Golden SHA-256 of the background of every PIC (with overlays where rooms use them): HD RGBA at N ∈ {1, 2, 5, 10}, original palette-index frame at N = 1; equal on x86-64 and ARM64 | R-56, R-57 | FULL | — | `tools/golden-hashes/main.cpp:58`; `tests/golden/pictures.txt` (88 pictures, 440 `pic` + 35 `stack` lines) | local (claimed; ARM64 under qemu-user per `reports/milestone-3b.md`). What the spec does not ask for is also absent (HD priority map, N > 10, views, fonts, composed frames: V-04) — a stage 5 safety-net question, not a gap. |
| G-55 | §5.3 Rules enforced by clang-tidy and by cross-architecture golden tests | R-54, R-55 | FULL | — | `.clang-tidy`; `tests/golden/synthetic_test.cpp` | |

## §6 Graphics

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-56 | §6.1 Default N = largest that fits, in physical pixels (display scaling); user can choose N; letterbox | R-112 | FULL | — | `main.cpp:812` (`SDL_GetWindowPixelDensity`), `:1108` (high pixel density), `:734-739` | untested (app layer). |
| G-57 | §6.1 Picture area 320 x 190 at offset 10; menu bar rows drawn as interface | R-87 | FULL | — | `docs/hd-composition.md:114-131`; `tests/unit/platform/interface_check_test.cpp:121-125` | |
| G-58 | §6.1 Original mode: EGA palette and dithering at any N; sprites occluded by the 1x priority map in N x N blocks; bit-identical at N = 1 | R-19, R-111 | FULL | — | `src/sci-render/include/sci/render/original.hpp:17`; `tests/unit/render/render_test.cpp:135` | N = 1 identity with the reference is local (claimed). |
| G-59 | §6.1 Original wins in original mode, spec in HD mode; game logic never changed | R-25 | FULL | — | `compose.cpp:151`, `:207` (HD priority used for drawing only) | |
| G-60 | §6.2 Path 1: port of ScummVM's algorithms, visual/priority/control maps plus 1x pair map; game logic reads path 1; bit-identical to ScummVM as palette-index buffers | R-18, R-19, R-25 | FULL | — | `src/sci-pic/src/draw_pic.cpp`, `canvas.cpp:62-65`; `tests/unit/pic/draw_pic_test.cpp:46-158`; `tools/graphics-compare/run.sh` | Bit-identity local (claimed). |
| G-61 | §6.2 Path 2: vector scene rasterised at 320N x 190N | R-26, R-50 | FULL | — | `src/sci-vector/src/build_scene.cpp:1262-1270` | |
| G-62 | §6.3 Lines from PIC end points; every visible line a 1-pixel antialiased stroke, not thinned | R-40 | FULL | — | `src/sci-vector/include/sci/vector/scene.hpp:66-71`; `render_test.cpp:81` | |
| G-63 | §6.3 Curve chains fitted as arcs, ellipses or splines by least squares in double, threshold about 0.5–1 px; ellipses not rounded to circles | R-38, R-39 | FULL | — | `build_scene.cpp:33-40`; `src/sci-vector/src/fitting.cpp:21`; `tests/unit/vector/fitting_test.cpp:53-103` | 0.5 px (customer, milestone 3 gate); 1 px for hinted chains. Open elliptic arcs and rotated ellipses become splines (`docs/ASSUMPTIONS.md:793-800`). |
| G-64 | §6.3 Solid brushes as exact shapes; random-pattern brushes as the scaled pattern | R-41 | FULL | — | `tests/unit/vector/build_scene_test.cpp:166` | |
| G-65 | §6.3 Snapping of near end points (1–2 px) and T-junctions; planar graph; face colour from the pair map at an interior point; thin faces fall back; geometric fills | R-26, R-28, R-30 | FULL | — | `build_scene.cpp:42-44`, `:134`; `build_scene_test.cpp:86`, `:98`, `:158` | Colour is the majority of the face's region pixels outside the edge band (documented variant, `docs/ASSUMPTIONS.md:764-770`). |
| G-66 | §6.3 Gap rule; original leaks kept as the player saw them | R-28, R-29 | FULL | — | `build_scene_test.cpp:115` | |
| G-67 | §6.3 Drawing order kept as a stack of layers | R-40 | FULL | — | `scene.hpp:66-71` | |
| G-68 | §6.3 Debug mode highlighting every replacement, development only, behind a CMake option | — | FULL | — | `CMakeLists.txt:17`; `build_scene.cpp:1328`; `tools/room-viewer/main.cpp:132` | Not built in any CI preset **[not verified that it still compiles]**. |
| G-69 | §6.4 Unresolvable regions fall back to 1x shapes in N x N blocks; the replacement report lists every fallback | R-30, R-48, R-128 | PART | Low | `tools/hd-report/main.cpp:237-238`; `tests/unit/render/render_test.cpp:117`; `docs/ASSUMPTIONS.md:782-787` | Mixed faces are listed; thin faces (the majority of fallbacks) are only counted. |
| G-70 | §6.4 Acceptance metric: pair classes, no swaps, ≥ 99 % outside a 1 px band; every visible line covered along its length | R-45 | FULL | — | `src/sci-render/include/sci/render/acceptance.hpp:1-39`; `reports/milestone-3b.md:78-83` | local (offline report; `acceptance.cpp` in no CI test). Worst 99.993 %, 0 swaps, 0 untouched line pixels (claimed). "Swap" was left undefined by the spec: G-125. |
| G-71 | §6.4 Manual side-by-side acceptance at the milestone 3 gate | — | FULL | — | `docs/ASSUMPTIONS.md:843-858`, `:1059-1068` | Approved with the 3b gate. |
| G-72 | §6.5 Pairs through the active palette (4 x 40); dithering replaced by a 256-entry LUT computed at build time; dither rule verified, ScummVM's undithering cross-checked and differences documented | R-18, R-24, R-47 | FULL | — | `colors.hpp:43-55` (`constexpr` table); `docs/ASSUMPTIONS.md:810-814` | |
| G-73 | §6.5 Plain sRGB average, 16 bits per channel, sums in `uint64_t`, shifts with explicit rounding; edge coverage in the same space; no linear light | R-46, R-47 | PART | Low | `colors.hpp:36-66`; `src/sci-render/src/rasterize.cpp:181-195`, `:374` | Sums are `uint32_t`, not `uint64_t`. No overflow: at most 256 samples x 2 x 255 x 257 = 33,553,920 < 2^32 (arithmetic from the code). Deviation in letter only. |
| G-74 | §6.5 Output 8-bit sRGB, round half up; no output dithering | R-46 | FULL | — | `colors.hpp:57-60` | |
| G-75 | §6.6 16 x 16 supersampling; every sample in exactly one face; a fitted curve shared by both faces; weight = sample count | R-26, R-46 | FULL | — | `src/sci-render/include/sci/render/rasterize.hpp:1-8`; `build_scene_test.cpp:217` | |
| G-76 | §6.6 Interior fast path; per-scanline coverage; tiles in parallel, independent of thread count | R-51 | FULL | — | `docs/hd-pipeline.md:190-194`; `rasterize.cpp:374-380` | Tiles are pixel rows. |
| G-77 | §6.6 Exact area coverage (optional, old milestone 12 → part of A4 milestone 10) | — | DECL | none | — | Declined with milestone 10 (2026-11-05). |
| G-78 | §6.6 HD priority map from the vectors, smooth occlusion edges by default; a switch snaps to the sprite grid | R-49, R-83 | FULL | — | `rasterize.hpp:66-70`; `compose.cpp:151`, `:207`; `compose_test.cpp:70`, `:90` | `priority_snap`. |
| G-79 | §6.7 Hint files: one per PIC resource in `hints/`, versioned format, committed schema validated in CI; content: what regions and chains represent, intended primitive, symmetry and repetition, accidental gaps | R-32, R-37 | PART | Low | `hints/` (86 files), `hints/schema.json`, `scripts/check-hints.py` | Pictures 55 and 992 have no hint file (88 pictures; `docs/ASSUMPTIONS.md:909`) — they needed none, but the spec says one per PIC. Region (508) and symmetry (27) hints are stored but used by nothing (allowed: §6.7 "Use" names chains and gaps only). |
| G-80 | §6.7 Anchoring to PIC command indices and 1x coordinates; PIC hash per file; local anchor check; mismatching file skipped and logged at run time | R-31, R-129 | FULL | — | `src/app/platform/src/picture_hints.cpp:25-38`; `tests/unit/vector/hints_test.cpp:112` | |
| G-81 | §6.7 Hints choose chains to fit and gaps to propose, within tolerance; every hint correction in the replacement report; off in original mode | R-33…R-35 | FULL | — | `build_scene.cpp:866`, `:896`, `:1126-1170`; `tests/unit/vector/hint_scene_test.cpp:60-93` | |
| G-82 | §6.7 Corrects, never invents; unsupported hints fall back | R-33, R-34 | FULL | — | `build_scene.cpp:1126-1170` | Hints are dropped if a curve misses a 1x line pixel or changes faces. |
| G-83 | §6.7 AI step offline; hints read as ordinary input; golden hashes for a fixed, versioned hint set | R-56, R-59 | FULL | — | `analysis_cache.cpp:50-78` (hint bytes in the key) | |
| G-84 | §6.7 (with A1) AI step only during development; hints frozen after approval (3b); customer spot-checks a sample and every uncertain hint | R-37 | FULL | — | `docs/ASSUMPTIONS.md:1067-1071` | The customer asked for no further spot check at 3b; 783 chains remain marked uncertain. |
| G-85 | §6.8 addToPic cels scaled N x N over the vector background; priority/control into path-1 maps, priority as N x N blocks in the HD priority map | R-83 | FULL | — | `docs/hd-composition.md:82-95`; `compose_test.cpp:129` | |
| G-86 | §6.8 Picture overlays as further layers in drawing order | R-56 | FULL | — | `src/app/platform/include/sci/platform/hd_pictures.hpp:3-9`; 35 `stack` goldens | |
| G-87 | §6.8 Kernel drawing primitives through the vector rasteriser; priority/control into path-1 maps; priority also drawn into the HD priority map | R-90 | PART | Low | `hd_pictures.hpp:37-38`; `docs/hd-composition.md:82-95`, `:186`; `tests/unit/vm/screen_test.cpp:202` | Lines become strokes; boxes are N x N blocks (identical for axis-aligned boxes). Their priority is the 1x priority in N x N blocks, not vector-drawn. |
| G-88 | §6.9 Sprites as cels scaled N x N (nearest); integer 1x positions | R-82 | FULL | — | `compose.hpp:45-63`; `tests/unit/vm/screen_test.cpp:120-164` | |
| G-89 | §6.9 Cursor sprite scaled N x N, positioned smoothly on screen pixels; game gets the 1x position | R-88, R-89 | FULL | — | `src/app/src/game_screen.cpp:193-240`; `tests/unit/app/game_screen_test.cpp:202-269` | |
| G-90 | §6.9 Fonts as original bitmap glyphs scaled N x N | R-23, R-87 | FULL | — | `compose.hpp:5-7` | |
| G-91 | §6.9 Vectorised glyphs (optional, old 11 → new 12) | — | DECL | none | — | Declined (2026-11-05). |
| G-92 | §6.9 Transitions (show styles) on N x N blocks | R-86 | FULL | — | `src/sci-vm/src/graphics.cpp` (1x); composed per 1x pixel (`docs/hd-composition.md:72-80`) | Not covered by a CI test of its own. |
| G-93 | §6.9 Composition order: background, occluded sprites, text and windows | R-82, R-87 | FULL | — | `compose.hpp:1-15`; `compose_test.cpp:116` | |

## A1 (§6.10, milestone 3b) and A2

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-94 | A1 plan: milestone 3b gate; hint schema covers depth/containment relations; hints frozen and goldens regenerated at 3b; release v0.1 after 3b | R-36 | FULL | — | `hints/schema.json`; `src/sci-vector/src/hints.cpp`; `docs/ASSUMPTIONS.md:1059-1073` | The release itself: G-117. |
| G-95 | A1 rules: line ends clipped at the contour they end on; equal neighbours' common boundary drawn once in the colour seen at 1x; no stray fragments; no breaks between connected strokes | R-42 | PART | Low | `src/sci-vector/src/stroke_integrity.cpp:16-35`; `tests/unit/vector/stroke_integrity_test.cpp:67-118` | Implemented as a pass; remaining cases (7,070 overshoots, 2,859 doubled boundaries, 6,436 fragments, 7,396 broken joints; `reports/milestone-3b.md:60-110`) were accepted by the customer as listed (option 1, `docs/ASSUMPTIONS.md:1063-1066`). Low: accepted, but visible in HD. |
| G-96 | A1 rules: nearer objects drawn over farther ones (depth from the priority map) | R-43, R-44 | PART | Low | `src/sci-render/include/sci/render/integrity.hpp:1-30` | Depth is only measured (15 pixels found), never used to reorder; layers keep the command order (R-44, HYP medium). Accepted at 3b. |
| G-97 | A1 evidence: depth and containment relations from the AI hints | R-36, R-44 | DROP | none | `hints.cpp` parses them; no user; no hint file has any | Dropped (answer 3 after stage 1b, confirmed). The stale claims that they are used (`src/sci-vector/include/sci/vector/hints.hpp:78-80`, `hints/README.md` "Relations … The vector path uses them from milestone 3b") are counted in G-123. |
| G-98 | A1: a fitted primitive that cannot satisfy the rules falls back to its original segments | — | DROP | none | — | "The fitted-chain fallback and a local 1x fallback are not implemented" (`docs/ASSUMPTIONS.md:1063-1066`, accepted at 3b); answer 4. |
| G-99 | A1 limits: only the HD picture changes; differences on the involved strokes within the 1 px band; no region changes its pair | R-42, R-45 | FULL | — | `reports/milestone-3b.md:78-83` | Acceptance still holds (claimed). |
| G-100 | A1: every clip, merge, removal and closed joint reported with its evidence; automated A1 checks over every picture | R-43, R-128 | FULL | — | `stroke_integrity.cpp:37-40`; `tools/hd-report/main.cpp:238` (`--changes`) | local. |
| G-101 | A2 sprite undithering (withdrawn 2026-10-28) | — | FULL | — | `grep -ri undither src` → only a comment on ScummVM's dither in `src/sci-pic/src/canvas.hpp:103` | Nothing of A2 is in the code: correct for a withdrawn addendum. |

## A4 (milestone 10, review format)

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-102 | A4 part A: object-level reconstruction of HD backgrounds (milestone 10) | — | DECL | none | not on `main` (branch `milestone-10`, deleted locally) | Declined 2026-11-05. |
| G-103 | A4 part B: visual review format (overview sheets with the A–C / 1–5 grid) for every visual review "from now on", including the audit | — | AMB | Low | `tools/hd-frames/main.cpp:209-230` makes side-by-side sheets without the grid; no grid tool on `main` (`grep -ri grid tools`) | The format is a "decision in force" for the audit (`review/STATUS.md`), yet its tooling was built only for milestone 10 and is not on `main`; answer 4 would drop it. Which applies decides whether stage 3/5 visual reviews need new tooling. |

## §7 VM, timing and input

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-104 | §7 Port SCI0 logic (resources, bytecode, classes, objects, selectors, kernel functions with calls logged, animation and motion, vocabulary and Said, windows and text) | R-63, R-64, R-81, R-85, R-86 | FULL | — | `src/sci-vm/src/*`; `src/sci-text/` | local (claimed scenarios). `kDoAvoider` is a stub that faults (R-65) but the game does not reference it (`docs/ASSUMPTIONS.md:1208`). |
| G-105 | §7 Port ScummVM's script patches for this game, per accepted release | R-69, R-70 | FULL | — | `src/sci-vm/src/script_patches.cpp:612-742`; `tests/unit/vm/script_patcher_test.cpp:105-192` | 16 patches, all active. Effect on the 5.25" release: untested scope (G-114). |
| G-106 | §7 Keep ScummVM's state model (reg segment:offset, segments, heap) | R-78 | FULL | — | `src/sci-vm/src/seg_manager.cpp`, `segments.cpp` | |
| G-107 | §7 Virtual clock and an RNG ported from ScummVM's random source with the same seeding; reference patched the same way | R-71, R-72, R-74 | FULL | — | `src/sci-vm/src/timing.hpp:3-112`; `k_math.cpp:41-59`; `tests/unit/vm/input_test.cpp:140-231`; `tools/scummvm-patches/0003-deterministic-trace.patch` | |
| G-108 | §7 One replay format for both engines; Event Recorder evaluated and decision recorded | R-77 | FULL | — | `src/sci-vm/src/events.cpp:184`; `docs/formats/replay-and-trace.md`; `docs/ASSUMPTIONS.md:2823-2830` | |
| G-109 | §7 Original tick rate; presentation at the display rate, repeating the last state | R-71, R-118 | FULL | — | `timing.hpp:3-12`; `main.cpp:654-685` | untested (pacing). |

## §8 Sound

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-110 | §8 Exactly two music devices from the game's sound resources; driver behaviour confirmed from game files and ScummVM; silent driver from milestone 4 matching ScummVM's timing, signals and cues | R-93, R-97 | FULL | — | `src/sci-vm/src/music.cpp:169-184`; `tests/unit/vm/music_test.cpp:81-201` | Songs with digital samples stop the machine (R-98); the game's 119 sounds all render (milestone 7), so none has one **[inferred from the golden list]**. |
| G-111 | §8 AdLib: Nuked OPL3 (cycle-accurate, integer) plus a software mixer; golden hashes of the OPL output | R-94, R-99, R-100 | FULL | — | `src/sci-audio/src/adlib.cpp`; `tests/golden/sounds.txt`; `tools/sound-render/main.cpp:17-29` | Goldens local; CI has one OPL note (`opl3_test.cpp:142`). |
| G-112 | §8 MT-32: mt32emu with user ROMs from a configured path, never bundled; automatic fallback to AdLib | R-95, R-96 | FULL | — | `src/app/platform/src/mt32_roms.cpp:49-89`; `main.cpp:1009-1027`; `tests/unit/platform/mt32_roms_test.cpp:63-102` | |
| G-113 | §8, §10.1 Audio output through SDL3 audio | R-100 | FULL | — | `src/app/src/audio_output.cpp:150-197` | |

## §9 Saves

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-114a | §9 Own versioned save format, described in docs/; save/restore for the whole game; older versions migrated or rejected with a clear message | R-103…R-109 | FULL | — | `src/sci-vm/src/save_game.cpp:46-147`; `docs/formats/save-format.md`; `tests/unit/vm/save_test.cpp:122-198` | Only version 1 exists; others are refused naming both versions. |
| G-114b | §9 ScummVM-compatible saves (old 10 → new 11) | — | DECL | none | — | Declined (2026-11-05). |

## §10 Application and platform

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-114 | §10.2 Other English DOS EGA releases from ScummVM's tables accepted with a warning and listed in ASSUMPTIONS; any other version stops | R-04, R-05, R-06 | FULL | none (untested scope) | `identify.cpp:66-70`, `:124-138`; `docs/ASSUMPTIONS.md:51-104` | The 5.25" release: **untested scope** (customer, answer 2 after stage 1b). Its warning says the HD hints are not used, which nothing enforces and is unverified (V-01). |
| G-115 | §10.1 SDL3 window, windowed and fullscreen; keyboard and mouse as in the original | R-119, R-120 | FULL | — | `main.cpp:470-473`, `:753-755`; `game_screen.cpp:60-190`; `game_screen_test.cpp:63-135` | |
| G-116 | §10.1 Dirty rectangles: only changed areas redrawn and uploaded | R-91 | FULL | — | `game_frame.cpp:34-60`; `tests/unit/platform/game_frame_test.cpp:98` | |
| G-116a | §10.2 Identification by resource hashes; the customer's copy is the tested release | R-02, R-03 | FULL | — | `identify.cpp:25-64` | |
| G-116b | §10.2 Configuration file next to the executable (INI/TOML, format version): game folder, mode, N, MT-32 ROM path | R-113, R-114 | FULL | — | `config.cpp:16-145`; `tests/unit/platform/platform_test.cpp:15-69` | Rewriting the file at exit drops unknown keys and user comments (R-114) and a failed write is silent — a quality issue, not a spec deviation; for stage 3. |
| G-116c | §10.2 Analysis cache: all rooms analysed on first launch with progress, cached next to the configuration, keyed by game files, analyser and hint-file versions; missing rooms analysed lazily; hints shipped with the program | R-59…R-62, R-124 | FULL | — | `src/app/platform/src/analysis_cache.cpp:50-140`, `:289-330`; `tests/unit/platform/analysis_cache_test.cpp:33-51` | The rule "bump `analyser_version` when the goldens change" is not enforced (R-58, V-12), Low, counted in G-128. |
| G-116d | §10.2 "next to the executable" for configuration (and cache, saves, game-written files) | R-125 | AMB | Medium | `main.cpp:1086-1087`, `:1125`, `:1137` | The spec fixes the location; on installed or read-only locations (Windows `Program Files`, a macOS app bundle, a Linux package) it cannot be written (V-08, **HYPOTHESIS** medium, not tested on any platform). The requirement itself conflicts with ordinary installs. |
| G-116e | §10.3 60 FPS up to N = 10 and room entry under 1 s on an ordinary laptop without a discrete GPU; backgrounds rasterised once per room | R-92 | PART | Medium | `reports/milestone-3.md:96-103`, `reports/milestone-5.md:140-153`, `reports/milestone-4b.md:127-129` | Measured only on the 4-core development container (no GPU; Linux): changed frames ≤ 1.4 ms, room entry ≤ 530 ms at N = 10, first launch 2.8 s; a whole frame at N = 10 takes 20.7 ms (> 16.7 ms; on room entry and mode switch only). Never measured on reference-class hardware, on Windows or on ARM64, and no automated performance check guards it. |
| G-116f | §10.3 Reference machine proposed at milestone 3 and confirmed; first-launch time budget | — | FULL | — | `docs/ASSUMPTIONS.md:831-838`, `:854-857` | 4-core 2020 laptop, room entry < 1 s at N = 10, first launch < 15 s. |

## §10.4, §14 Build and delivery

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-117 | §10.4, §12 (3a, moved after 3b by A1), §14 Unsigned release binaries for all three OSes built by CI, **including release v0.1**; delivery "source plus unsigned release binaries" | R-141 | PART | **High** | `.github/workflows/release.yml`; `scripts/package-release.sh:15-29` | The release workflow exists, but **no release was ever published**: `git ls-remote --tags origin` → none; `gh api repos/CityRunner/sierra-hd/releases` → `[]`. The tags `v0.1.0` and `v0.9.0-rc.1` exist only in the local clone (the session may not push tags: `reports/milestone-9.md:162-163`; `docs/ASSUMPTIONS.md:2881`). The release workflow last ran on pull requests in milestones 3a–4b (2 failures). `README.md` says binaries "are attached to the GitHub releases". macOS binaries cannot be built (G-07). A required deliverable is missing; the fix is the customer's tag push. |
| G-118 | §14 README: how to build, point the program at the game folder, configuration options, MT-32 ROMs | — | PART | Low | `README.md`; `docs/USER-README.md:74-92` | Build, game folder and MT-32 are covered; configuration options are in `docs/USER-README.md` (shipped as the release README), not in `README.md`. `README.md` also states a macOS build path that fails (G-07) and published binaries that do not exist (G-117). |
| G-119 | §14 THIRD_PARTY.md: every component with its licence, incl. ported ScummVM code | — | FULL | — | `THIRD_PARTY.md` | |
| G-120 | §14 Golden-hash files and test scripts, with instructions for the local tests that need game files | R-56, R-99, R-131 | FULL | — | `tests/golden/`; `tests/README.md`; `tools/*/README.md` | Four tools lack a README (MAP.md 8.13), Low, counted in G-123. |

## §11 Testing

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-121 | §11 CI without game files: synthetic PIC streams, numeric unit tests, fuzzing of parsers and rasteriser, hint schema; whole suite under the sanitizers; synthetic golden hashes on x86-64 and ARM64 runners | R-55, R-137, R-138 | FULL | — | `tests/unit/`, `tests/fuzz/fuzz_vector.cpp` (rasteriser), `tests/golden/synthetic_test.cpp`; `ci.yml:179-298` | Fuzz coverage gaps in G-48. As specified, CI leaves the VM nearly untested (MAP.md 8.1). |
| G-122 | §11 Local with game files: golden frame hashes (also ARM64 under qemu-user), comparison with ScummVM, hint-anchor check, deterministic runs; regression at N = 1 original mode bit-identical | R-19, R-56, R-76, R-129 | FULL | — | `tools/golden-hashes`, `tools/graphics-compare`, `tools/vm-trace/run-all-scenarios.sh`, `tools/hint-tool` | local (claimed; not re-run in stage 2). |

## §12 Milestones (exit criteria with checkable artefacts)

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-124a | M0 bootstrap: empty libraries build and pass style and sanitizer jobs on every CI platform; reference ScummVM runs; decisions recorded | — | FULL | — | `reports/milestone-0.md`; `docs/ASSUMPTIONS.md:606-611` | |
| G-124b | M1 every resource decodes byte-identically to ScummVM | R-15 | FULL | — | `tools/resource-compare/run.sh` | local (claimed). |
| G-124c | M2 path 1, views, fonts bit-exact for all rooms; room viewer | R-19, R-126 | FULL | — | `tools/graphics-compare/run.sh`; `tools/room-viewer/` | local (claimed). |
| G-124d | M3 original mode identical to M2; per-room replacement report checked against §6.4; HD goldens recorded | R-45, R-56, R-128 | FULL | — | `reports/milestone-3-pictures.md`; `tests/golden/pictures.txt` | |
| G-124e | M3a hints committed; release build of the room viewer without debug overlays (window, N, HD switch) | R-126 | FULL | — | `main.cpp:156-190` (`--viewer`); `docs/ASSUMPTIONS.md:955-970` | The CI binaries of v0.1: G-117. |
| G-124f | M4a boot, kernel-call traces match; M4b every referenced kernel function implemented, scenarios over the main areas match | R-64, R-76 | FULL | — | `tests/scenarios/scenarios.txt` (39 runs) | local (claimed). 97 kernel functions referenced; DoAvoider not among them. |
| G-124g | M5 scripted walks give ScummVM's hero positions frame by frame; §6.8 cases side by side | R-81 | FULL | — | `tools/vm-trace` actor files; `reports/milestone-5.md` | local (claimed). |
| G-124h | M6a typed commands give ScummVM's results; M6b window layout matches in 1x and renders at every N | R-85, R-86, R-87 | FULL | — | `tests/unit/text/parser_test.cpp`; `tools/hd-frames --check-interface` | local + CI (synthetic). |
| G-124i | M7 OPL goldens; every sound plays on both devices; signals and cues match; by-ear comparison recorded | R-97, R-99 | FULL | — | `tests/golden/sounds.txt`; `reports/milestone-7.md:196-200` | |
| G-124j | M8 save/restore at scenario points gives the same VM state hash; replay continues identically | R-109 | FULL | — | `tools/vm-trace/run-round-trips.sh`; `tests/unit/vm/save_test.cpp:64-82` | local (claimed) + CI (synthetic). |
| G-124k | M9 the agent's scripted playthrough in both graphics modes, start to end; optional walkthrough checkpoints match ScummVM | R-80 | FULL | — | `tests/playthrough/`; `reports/milestone-9.md:80-100` | local. |
| G-124l | M9 one full manual playthrough by the customer in HD mode, spot checks in original mode | — | DECL | none | `docs/ASSUMPTIONS.md:2879-2882` | Waived by the customer for the merge (2026-11-05). Nobody has played the merged program by hand end to end **[per the records]**. |

## §13 Working protocol and §15 open questions (checkable artefacts only)

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-125a | §13 A pull request and `reports/milestone-N.md` per milestone and sub-gate; decisions recorded in `docs/ASSUMPTIONS.md`; formats documented in `docs/`; licence headers kept | — | FULL | — | `reports/milestone-0.md` … `milestone-9.md`; `docs/formats/` | |
| G-125b | §15 Open questions: arc threshold, edge blend space, reference machine, AI tooling, optional walkthrough | — | FULL | — | `docs/ASSUMPTIONS.md:843-858` (0.5 px, sRGB, machine); `:898-911` (the agent, Claude, did the AI step); `tests/playthrough/` (scripted walkthrough) | Resolved. Hint licensing: G-24. |
| G-125c | §15 Whether other sound devices are wanted after the first release | — | AMB | Low | — | Unanswered; moot until a first release exists (G-117). |

## Contradictions and ambiguities in the requirements (not listed above)

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-126 | §1.1/§6.1 "Game logic is never changed" / "always behaves as in the original" vs §7 "Port the script patches ScummVM applies to this game" | R-69, R-70 | AMB | Low | `script_patches.cpp:612-742` | The 16 patches change the original game's logic (lock-up fixes, copy-protection random fix). The project followed ScummVM; the spec's two statements disagree. |
| G-127 | §6.4 "no swaps" and "pair classes" are not defined | R-45 | AMB | Low | `docs/ASSUMPTIONS.md:801-809` | Resolved by the agent's definitions (swap = 4+ connected differing pixels outside the band; (a,b) ≡ (b,a)), decided at milestone 3. |
| G-128a | N has no upper bound in §6.1, but §5.3 goldens stop at N = 10, §10.3 performance targets stop at N = 10, and the code allows N up to 16 | R-50, R-57 | AMB | Low | `config.hpp:36`; `compose.hpp:94`; `fitting.cpp:15-17` (V-03) | N = 11–16 has no golden, no performance target and a flattening comment that assumes N ≤ 10. |
| G-128b | Milestone numbers: the spec (10 saves, 11 glyphs, 12 exact coverage) vs A4 (10 HD quality incl. exact coverage, 11 saves, 12 glyphs) vs reports dated before 2026-10-29 | — | AMB | Low | `docs/SPEC_ADDENDA.md:8-14`; `docs/ASSUMPTIONS.md:2784-2800` | Resolved by the mapping table; recorded so the IDs here are unambiguous. |
| G-128c | Answer 4 ("If it is not implemented, then drop it") is an ASSUMED reading; its edge cases are open: macOS (G-07, partly built), unpublished release (G-117, built but not published), A4 part B (G-103) | — | AMB | Medium | `review/STATUS.md` | Medium: it decides whether three High/Low items are work or dropped. Stage 3. |

## Scope creep (implemented but not required)

| ID | What | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-129 | Test hooks in the shipped program: `--replay`, `--seed`, `--fast`, `--end-state` | R-115…R-117 | CREEP | Low | `main.cpp:73-100`, `:687-728`, `:1030-1055`, `:1143-1169` | Serve the milestone 9 playthrough in the program (both graphics modes); §5.1 "no debug hacks outside build options" argues for a build option or a tool. Untested by CTest (R-110). |
| G-130 | `volume` key and `music = off` | R-96, R-102 | CREEP | Low | `config.hpp:35-56`; `main.cpp:1012`, `:1026-1027`; `mixer_test.cpp:126` | Small, harmless conveniences beyond §10.2's key list. |
| G-131 | `SCI_WITHOUT_STROKE_INTEGRITY` build option | R-42 | CREEP | Low | `build_scene.cpp:1257-1261` | Kept from the 3b baseline measurement; a second code path no CI preset builds. |
| G-132 | Region, symmetry and relation hint parsing/validation kept although nothing uses them | R-36 | CREEP | Low | `src/sci-vector/src/hints.cpp`; `hints.hpp:78-100` | Regions and symmetry are spec content (§6.7) but consumed by nothing; relations are dropped (G-97). Parse code and 535 hint entries with no effect on output. |
| G-133 | Room viewer inside the product (`--viewer`) | R-126 | CREEP | — | `main.cpp:156-190` | Not creep in effect: §12 (3a) asks for "a release build of the room viewer" as v0.1; kept as a mode of the game program. Listed for completeness, no severity. |

## Cross-cutting documentation item

| ID | Requirement | R | Class | Sev | Code / test | Note |
|---|---|---|---|---|---|---|
| G-123 | §5.1 comments explain why; §14 docs current | — | PART | Low | `hints.hpp:78-80`, `hints/README.md` (relations "used from milestone 3b"); `src/sci-vm/README.md:7`, `:11`; `cmake/SciThirdParty.cmake:5-6`; `docker/run.sh:8`; `docs/ASSUMPTIONS.md:28` (private); `config.cpp:184` (V-11); `trigonometry.hpp:3-7` (V-05); `docs/formats/replay-and-trace.md:40` (V-07); four tools without README (MAP.md 8.13) | Stale or false statements in code comments and docs other than ARCHITECTURE.md (G-44). |
| G-128 | Unenforced rules and minor comment mismatches carried from stage 1b: V-03, V-05, V-12 | R-58, R-73 | PART | Low | `fitting.cpp:15-17`; `trigonometry.hpp:3-7`; `build_scene.hpp:97-100` | No script checks the analyser-version rule; two comments contradict the code. |

---

## Summary

### Counts per class and severity

DRAFT — recomputed in the final pass.

### Top findings

DRAFT — see the final pass.
