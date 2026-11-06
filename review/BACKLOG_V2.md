# Stage 4: improvements and iteration 2 backlog

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-06.
Read-only: nothing outside `review/` was changed.

Inputs: `review/MAP.md`, `review/REQUIREMENTS_RECOVERED.md` (R-, V-),
`review/GAPS.md` (G-), `review/QUESTIONS.md` (Q-, batch 1 with answers),
`review/HISTORY_CLEANUP.md` and every decision in force in
`review/STATUS.md`. The interim refactoring plan for stage 5 is
`review/REFACTOR_PLAN.md`; it cites the B-items below.

## How to read this file

**Kind:** requirement (new or changed behaviour for iteration 2, with
acceptance criteria), defect, tech debt, architecture, performance,
maintainability.

**Severity** (the brief's scale): Critical — broken core functionality,
data loss, or blocks iteration 2; High — significant deviation from
requirements or serious risk; Medium — noticeable quality or
maintainability issue; Low — cosmetic or minor. No item is Critical:
nothing found breaks the game or loses data beyond B-04's ini edits, and
nothing blocks iteration 2 once the stage 5 plan has run.

**Effort:** S = up to 4 hours, M = 4 to 16 hours, L = more than 16 hours,
each with an hour estimate for one agent working in the toolchain
container. The estimates are judgement, not measured; game-dependent
checks add their run time (not recorded so far, Q-01).

**Risk:** what could break while doing the item, and how it would show.

**State:**
- **Decided** — the customer decided it (answer named).
- **ASSUMED** — taken from a default the customer did not confirm
  (Q-03).
- **default — accept or strike** (none left: all decided by the customer
  on 2026-11-06, "2 all approved") — a batch-2 candidate or a PART
  remainder (Q-05) that was never asked. It carries the default I would
  apply. The customer accepts or strikes it at this STOP; silence means
  accept.

**When:** "stage 5" means the item is a step of `review/REFACTOR_PLAN.md`
(behaviour-preserving, before iteration 2); "iteration 2" means after
stage 5, as a behaviour change or as new work.

**Stage 5 bugs.** The brief forbids fixing bugs found during stage 5. Any
such bug is added below as B-43 onward, kind "defect", with the step that
found it.

## Overview

| B | Title | Kind | Sev | Effort | When | State | Refs |
|---|---|---|---|---|---|---|---|
| B-01 | Drop macOS completely | requirement | High | M (6 h) | stage 5 (build/CI/docs) | Decided | G-07, R-143, G-126 |
| B-02 | Logical repository layout | requirement | Medium | M (8 h) | stage 5 | Decided | Q-07, answer 5 (1a), G-29, G-18 |
| B-03 | One run-time data folder | requirement | Medium | M (14 h) | iteration 2 | Decided | Q-06, G-122, R-125, V-08 |
| B-04 | Ini updated in place | requirement (defect) | Medium | S (4 h) | iteration 2 | Decided | Q-10, R-114, V-08, V-11 |
| B-05 | Fuzz targets for scripts, sound, patch resources, ini | requirement | Medium | M (8 h) | stage 5 | Decided | Q-09, G-48, R-138 |
| B-06 | VM in CI: test assembler and hand-built bytecode tests | requirement | High | L (32 h) | iteration 2 | Decided | Q-01, MAP 8.1, G-03 |
| B-07 | VM in CI: a small home-made game | requirement | High | L (40 h) | iteration 2 | Decided | Q-01, MAP 8.1 |
| B-08 | sci-vm line coverage in CI ≥ 70 %, local VM gate until then | requirement | High | S (4 h) | stage 5 (gate) / iteration 2 (CI) | Decided | Q-01, MAP 6.4 |
| B-09 | No frame over 16.7 ms at N = 10 (measured misses only) | performance | Medium | M (16 h) | iteration 2 | Decided | Q-04, G-123, R-92 |
| B-10 | Scripted timing mode; customer's reference-laptop run | requirement | Medium | M (6 h) | iteration 2 | Decided | Q-04, G-123 |
| B-11 | Peak memory ≤ baseline + 10 % (regression check) | performance | Low | S (3 h) | iteration 2 | Decided | Q-04 |
| B-12 | Hint licensing section in `hints/README.md` | requirement | High | S (2 h) | stage 5 (docs) | Decided (Q-03) | Q-03, G-24, HISTORY_CLEANUP E |
| B-13 | `docs/ARCHITECTURE.md` brought up to date | maintainability | Medium | M (10 h) | iteration 2 | accepted (customer, 2026-11-06) | G-44, MAP 8.4 |
| B-14 | Other stale docs, comments and missing READMEs | maintainability | Low | M (6 h) | iteration 2 | accepted (customer, 2026-11-06) | G-155, G-156, G-39, G-126, MAP 8.5, 8.6, 8.13, V-03, V-05, V-07, V-11 |
| B-15 | Policy script and `.gitignore`: SCI0 patch files, run-time folders | defect | Medium | S (2 h) | stage 5 | Decided (Q-07) | G-18, V-06, R-135 |
| B-16 | Release workflow kept working without macOS; publishing is the customer's | requirement | Medium | S (3 h) | stage 5 (macOS part) / iteration 2 | Decided | G-125, R-141 |
| B-17 | `kDoAvoider` stub: scan other releases if a copy turns up | defect (latent) | Low | S (1 h) | iteration 2, conditional | accepted (customer, 2026-11-06) | R-65, R-66 |
| B-18 | Golden outputs for all resources | maintainability (safety net) | High | M (16 h) | stage 5 step 0 | Decided | Q-02, V-04, R-57, MAP 8.3 |
| B-19 | Hashes of every VM output | maintainability (safety net) | High | M (10 h) | stage 5 step 0 | Decided | Q-01, MAP 6.3 |
| B-20 | Baselines: warnings, sanitizers, timings, memory, size, build time | maintainability (safety net) | Medium | M (6 h) | stage 5 step 0 | Decided | Q-04, brief 5.1 |
| B-21 | Remove the four unused dependency edges | architecture | Low | S (1 h) | stage 5 | accepted (customer, 2026-11-06) | G-31, MAP 1.3, 8.7 |
| B-22 | Check in lint that every declared edge is used | maintainability | Low | S (3 h) | iteration 2 | accepted (customer, 2026-11-06) | G-31, MAP 1.4 |
| B-23 | Tests that include private `src/` folders | architecture | Low | S (2 h) stage 5; M (12 h) iteration 2 | stage 5 (make explicit) / iteration 2 | stage 5 part: plan; rest accepted (customer, 2026-11-06) | G-31, MAP 1.4, 8.9 |
| B-24 | Remove the dead `component_name()` placeholders | tech debt | Low | S (0.5 h) | stage 5 | accepted (customer, 2026-11-06) | G-43, MAP 8.8 |
| B-25 | Check the vendored files' SHA-256 | maintainability | Medium | S (3 h) | stage 5 | plan | MAP 5, 8.11 |
| B-26 | SPDX headers on scripts; policy checks `.py`/`.sh` | maintainability | Low | S (1.5 h) | stage 5 | plan | MAP 8.10, R-135 |
| B-27 | A reason on every `NOLINT` | maintainability | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | G-36, R-136 |
| B-28 | Switchable log categories in the program | requirement | Low | S (4 h) | iteration 2 | accepted (customer, 2026-11-06) | G-41, V-10, R-121 |
| B-29 | The range of N (cap or keep 16) | requirement | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | G-148, V-03, R-50 |
| B-30 | Other sound devices | requirement | Low | — (0 h with the default) | — | accepted (customer, 2026-11-06) | G-145 |
| B-31 | Hint files for pictures 55 and 992 | maintainability | Low | S (0.5 h) | iteration 2 | accepted (customer, 2026-11-06) | G-79 |
| B-32 | Make the application layer testable (options, `GameHost`) | tech debt | Medium | M (14 h) | iteration 2 | accepted (customer, 2026-11-06) | MAP 8.2, R-110, MAP 6.4 |
| B-33 | `-fwrapv` in some build types only | tech debt | Low | S (3 h) | iteration 2 | accepted (customer, 2026-11-06) | MAP 8.12 |
| B-34 | Test hooks in the shipped program | requirement | Low | S (1 h) | iteration 2 | accepted (customer, 2026-11-06) | G-151, R-115…R-117 |
| B-35 | Remove the unbuilt `SCI_WITHOUT_STROKE_INTEGRITY` option | tech debt | Low | S (1 h) | iteration 2 | accepted (customer, 2026-11-06) | G-153 |
| B-36 | Remove relation-hint parsing; fix its comments | tech debt | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | G-154, G-97, V-02, R-36 |
| B-37 | List thin fallbacks in the replacement report | requirement | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | G-69, R-128 |
| B-38 | Check the `analyser_version` rule in CI | maintainability | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | V-12, G-156 |
| B-39 | Trigonometry outside -360…360: compare with glibc | defect (unverified) | Low | S (2 h) | iteration 2 | accepted (customer, 2026-11-06) | V-05, G-156 |
| B-40 | 5.25" release: hint warning possibly false | defect (unverified) | Low | — | — | struck (customer, 2026-11-06) | V-01, V-14, R-06 |
| B-41 | LGPL static linking note | maintainability | Low | — | — | struck (customer, 2026-11-06) | V-09 |
| B-42 | `uint32_t` sums instead of `uint64_t` | tech debt | Low | — | — | struck (customer, 2026-11-06) | G-73, R-46 |

B-43 onward: reserved for bugs found in stage 5.

## Iteration-2 requirements decided by the customer

### B-01 Drop macOS completely

- **Kind:** requirement. **Severity:** High (G-07 High: an in-scope
  platform that did not build; now out of scope). **Effort:** M, 6 h.
  **When:** stage 5 (no program behaviour on Linux or Windows changes).
  **State:** Decided (answer 1 after stage 2, "drop macOS completely").
- **Refs:** G-07, G-126, R-143, R-141; MAP 4 (CI, release).
- **What is there today** (grep of the tree outside `third_party/`,
  `reports/`, `review/`):
  - `.github/workflows/ci.yml`: the `macos` dispatch option (`:34`), the
    plan output and its case (`:74`, `:107`, `:122`, `:135`), the job
    `macos` (`:372-403`), header comments (`:14`, `:17`).
  - `.github/workflows/release.yml`: header (`:6-8`), the `macos` input
    (`:15-18`), the job `macos` (`:110-142`). `publish` needs only
    `[linux, windows]` (`:146`), so it is unaffected.
  - `.github/actions/native-toolchain/action.yml`: description (`:3`), the
    macOS branch (`:78-80`, `MACOSX_DEPLOYMENT_TARGET` `:152`).
  - `CMakePresets.json:16-24`: display names "(Linux, macOS)".
  - `cmake/SciCompilerFlags.cmake:48` (`NOT APPLE` around `-fuse-ld=lld`),
    `:59` (message "Linux and macOS only").
  - `scripts/ci-minutes.py:34`, `:65-66` (macOS price and label);
    `scripts/package-release.sh:7` (comment).
  - `README.md:36-40` (macOS build steps), `:75` ("Linux, Windows and
    macOS").
  - `src/sci-vector/src/build_scene.cpp:499`: a `NOLINTNEXTLINE` whose
    reason is Apple's libc++ (no `std::ranges::iota`). Comment only; the
    code it guards stays as it is in stage 5 (changing it to
    `std::ranges::iota` is a code change, iteration 2 under B-27).
  - `src/app/src/game_screen.hpp:35`, `docs/hd-composition.md:144`: HiDPI
    remarks naming macOS as an example. They describe a general
    pixel-density case (Wayland and Windows scaling have it too), so they
    stay, reworded to not name macOS.
  - `docs/ARCHITECTURE.md:320` (libc++ on macOS), `docs/ASSUMPTIONS.md`
    (macOS decisions at `:114`, `:128`, `:137`, `:163-168`, `:235-264`,
    `:324-327`, `:360-389`, `:410`, `:436`): the decision log is history
    and is not rewritten; a new entry records the drop.
  - `third_party/SDL3`: Apple back-ends (`src/video/cocoa`,
    `src/video/uikit`, `src/audio/coreaudio`, `src/haptic/darwin`,
    `src/joystick/darwin`, `src/misc/macos`, 44 `.m` files) are part of the
    upstream copy, which `third_party/SDL3/VENDORED.md` says is "copied
    unchanged". **Decided (customer, 2026-11-06, "2 all approved"): keep them.** SDL's own CMake
    builds them only on Apple; removing them saves no build time on Linux
    or Windows, breaks "copied unchanged" and makes the next SDL update a
    manual diff. If struck (prune), VENDORED.md lists the removed paths and
    B-25's checksums cover the rest.
- **Acceptance criteria:**
  1. `git grep -niE 'macos|apple|darwin|xcode|osx'` outside `third_party/`,
     `reports/` (`docs/reports/` after B-02), `review/` and the dated
     entries of `docs/ASSUMPTIONS.md` finds nothing but the new decision
     entry.
  2. `ci.yml` and `release.yml` have no macOS job, input or plan output;
     `actionlint` (or a dry parse with `gh workflow view`) accepts both
     files; a manual CI run with each remaining option (`quick`,
     `linux-arm64`, `windows`) still starts the same jobs as before.
  3. The release workflow still builds Linux x64/arm64 and Windows
     x64/arm64 packages (B-16).
  4. `README.md` names Linux and Windows as the platforms; `docs/ASSUMPTIONS.md`
     has an entry "macOS dropped (customer, after stage 2)" pointing to
     G-07.
  5. Linux and Windows builds give the same binaries' outputs: all golden
     and VM hashes of B-18/B-19 unchanged; CTest passes in every remaining
     preset.
- **Risk:** low. A YAML slip disables a CI job silently: criterion 2.
  Removing `NOT APPLE` from the `-fuse-ld=lld` condition must keep it
  exactly "not clang-cl" (Linux behaviour unchanged).

### B-02 Logical repository layout

- **Kind:** requirement. **Severity:** Medium (the customer's request,
  answer 5 after stage 1a; it also blocks B-03, which needs `local/`).
  **Effort:** M, 8 h. **When:** stage 5 (pure moves plus placeholders; see
  `review/REFACTOR_PLAN.md` for why it is not deferred). **State:**
  Decided (Q-07 default).
- **Refs:** Q-07, Q-06, G-29, G-18, G-39, MAP 1.1.
- **Before → after** (`←` moved, `+` new; everything not shown is
  unchanged):

```text
BEFORE (main a0422d6)                    AFTER
sierra-hd/                               sierra-hd/
├── .github/                             ├── .github/              (paths edited)
├── cmake/                               ├── cmake/
├── docker/                              ├── docker/
├── docs/                                ├── docs/
│   ├── formats/                         │   ├── formats/
│   ├── release-notes/                   │   ├── release-notes/
│   └── *.md                             │   ├── reports/          ← reports/
├── hints/                               │   │   ├── milestone-*.md, README.md
├── reports/                             │   │   └── images/ dumps/  (git-ignored, local)
│   ├── milestone-*.md, README.md        │   └── *.md
│   └── images/ (git-ignored)            ├── hints/
├── review/                              ├── review/               (stays while the audit runs)
├── scripts/                             ├── scripts/
├── src/                                 ├── src/
│   ├── sci-core/ … sci-vm/  (9 libs)    │   ├── sci-core/ … sci-vm/  (9 libs)
│   └── app/                             │   ├── sci-platform/      ← src/app/platform/
│       ├── CMakeLists.txt               │   │   ├── CMakeLists.txt, include/, src/
│       ├── README.md                    │   │   └── README.md      + (G-39)
│       ├── platform/                    │   └── app/               the program only
│       │   ├── CMakeLists.txt           │       ├── CMakeLists.txt
│       │   ├── include/sci/platform/    │       ├── README.md
│       │   └── src/                     │       └── src/
│       └── src/                         ├── tests/
├── tests/                               ├── third_party/
├── third_party/                         ├── tools/
├── tools/                               ├── local/                 + git-ignored except placeholders
└── (top-level files)                    │   ├── README.md          +   what goes where; never commit
                                         │   ├── game/.gitkeep      +   your game files
                                         │   ├── rom/.gitkeep       +   MT-32 / CM-32L ROMs
                                         │   └── saves/.gitkeep     +   save games
                                         │   (sierra-hd.ini, cache/, files/ appear here
                                         │    once B-03 makes local/ the development data folder)
                                         └── (top-level files)
```

  The include path `sci/platform/…` and the CMake target `sci-platform`
  do not change, so no `#include` changes. The executable stays at
  `build/<preset>/src/app/sierra-hd`.
- **Every reference that moves with it** (from `git grep` on `a0422d6`):
  - **CMake:** `CMakeLists.txt:46` gains `add_subdirectory(src/sci-platform)`
    before `src/app`; `src/app/CMakeLists.txt:2` loses
    `add_subdirectory(platform)`. `src/app/platform/CMakeLists.txt` moves
    unchanged (its paths are relative). No test or tool CMake file names
    `src/app/platform` (they link the target `sci-platform`).
    `tests/unit/CMakeLists.txt:96-110` names `src/app/src`, which stays.
  - **CI:** `ci.yml:19`, `:28` (`paths-ignore: "reports/**"`), `:100`
    (`docs/* | reports/*`): `reports/` becomes part of `docs/`, so the
    `reports` entries are deleted (the `docs/**` rule covers them).
  - **`.gitignore`:** `/reports/images/`, `/reports/dumps/` →
    `/docs/reports/images/`, `/docs/reports/dumps/`; new
    `/local/*` with `!/local/README.md`, `!/local/*/` and
    `!/local/*/.gitkeep` (exact rules checked with `git check-ignore -v`).
  - **Policy script:** `scripts/check-repo-policy.py:56` (message naming
    `reports/images/`); new rule: under `local/` only `README.md` and
    `*/.gitkeep` may be tracked (closes G-18 for that folder; B-15 covers
    the rest).
  - **Scripts and tools:** `tools/room-viewer/main.cpp:25` (comment),
    `tools/room-viewer/README.md:26`, `tools/hd-frames/README.md:54`,
    `tools/hd-report/README.md:30` (`--images reports/images/hd`). Local
    check scripts that take a game folder keep their arguments; their
    examples and the documented mount use `local/game` instead of
    `/home/user/game/...` (`docs/ASSUMPTIONS.md:40-42` gets a new entry,
    not an edit).
  - **Docs:** `README.md:17`, `:103`, `:111`; `docs/ARCHITECTURE.md`,
    `docs/SPEC_ADDENDA.md`, `docs/ASSUMPTIONS.md` (34 lines name
    `reports/` outside `review/`, 2 name `src/app/platform`; each becomes
    `docs/reports/…` or `src/sci-platform/…`); `reports/README.md` and the
    milestone reports' own mentions of `reports/`. Reports have no relative
    Markdown links (`git grep '](' reports`: none), so no link breaks.
  - **Not edited:** `review/*.md` cite `file:line` on the audited commit
    `a0422d6`; they stay as written, with one note at the top of
    `review/STATUS.md` giving the path mapping. `THIRD_PARTY.md:59` names
    `src/app/src/game_screen.cpp`, which does not move.
- **Acceptance criteria:**
  1. `git grep -n 'src/app/platform'` and `git grep -nE '(^|[^/])reports/'`
     find hits only in `review/` and in dated decision-log entries.
  2. `git status --ignored` after a run shows `local/game/*`,
     `local/rom/*`, `local/saves/*` ignored; `git ls-files local` lists
     exactly `local/README.md` and the three `.gitkeep` files.
  3. The policy script fails on a test file `local/game/RESOURCE.000`
     forced into the index (`git add -f`), and passes without it.
  4. Every preset configures and builds; CTest passes; B-18/B-19 hashes
     unchanged; `git log --follow` works on a moved file.
- **Risk:** low-medium. Absolute source paths reach the binary through
  `std::source_location` in `SCI_ASSERT` messages
  (`src/sci-core/include/sci/core/assert.hpp:27`), so sci-platform's object
  files change; only an assertion message's file name differs, never a
  normal run's output. Binary hashes are therefore not the check; the
  output goldens are. Open pull requests or branches that touch moved files
  get conflicts (none are open as far as this audit knows).

### B-03 One run-time data folder

- **Kind:** requirement (behaviour change). **Severity:** Medium (G-122
  AMB Medium; V-08). **Effort:** M, 14 h. **When:** iteration 2.
  **State:** Decided (Q-06 default).
- **Refs:** Q-06, G-122, R-125, R-114, V-08, B-02 (`local/`), B-16.
- **Behaviour:** the program resolves one data folder:
  1. `--data DIR` (new option, also accepted with `--viewer`);
  2. else the executable's folder, if it holds `sierra-hd.ini` or `game/`
     and is writable ("portable": today's behaviour and the unpacked
     release archive);
  3. else the per-user folder: `%APPDATA%\sierra-hd` on Windows,
     `$XDG_DATA_HOME/sierra-hd` (default `~/.local/share/sierra-hd`) on
     Linux, created on first use.

  It holds `sierra-hd.ini`, `game/`, `rom/`, `saves/`, `cache/`, `files/`.
  An empty `game_folder` means `game/`; MT-32 ROMs are searched in `rom/`
  when `mt32_rom` is empty; `hints/` stays next to the executable.
- **Acceptance criteria:**
  1. Unit tests (CTest, no game data) for the resolution order with
     temporary folders: `--data` wins; portable when the exe folder has
     the ini or `game/` and is writable; per-user otherwise; a read-only
     exe folder falls through to per-user (Linux: `chmod a-w`; Windows:
     ACL in the Windows CI job, or documented as checked by hand).
  2. A folder given on the command line is used for that run and not
     saved (with B-04).
  3. `scripts/package-release.sh` puts empty `game/`, `rom/`, `saves/`
     (each with a short `README.txt`, never game data) into every archive;
     a test of the script lists the archive and checks them.
  4. Unpacking the archive, copying the game into `game/` and starting the
     program with no argument runs the game (local check with the
     customer's copy, Linux; the Windows run is part of the customer's
     B-10 laptop run).
  5. From a development build, `sierra-hd --data local` uses `local/`
     (B-02); `tools/playthrough/program-run.sh` uses `--data` instead of
     its symlinked run folder (`program-run.sh:71-76`).
  6. `docs/USER-README.md`, `--help` and a `docs/ASSUMPTIONS.md` entry
     describe the order; §10.2's "next to the executable" is recorded as
     replaced (G-122 closed).
  7. All B-18/B-19 hashes unchanged (no output change).
- **Risk:** medium. Existing users' ini, saves and cache sit next to the
  executable; rule 2 keeps finding them (the ini is there). A user whose
  exe folder is writable but holds neither file gets the per-user folder on
  first start (intended, new). Path handling on Windows (non-ASCII user
  names) is a known trap: test with a non-ASCII temp path.

### B-04 Ini updated in place

- **Kind:** requirement (fixes a small data loss). **Severity:** Medium
  (user edits lost silently). **Effort:** S, 4 h. **When:** iteration 2.
  **State:** Decided (Q-10 default).
- **Refs:** Q-10, R-114 (`config.cpp:163-190`, `main.cpp:1093`,
  `:1133-1134`, `:1171`), V-08, V-11 (`config.cpp:184` writes a wrong
  comment into the user's file).
- **Acceptance criteria:**
  1. The program writes the ini only when a value changed.
  2. A round-trip test: a file with comments, blank lines, unknown keys and
     a changed `mode` keeps every other line byte for byte, and only the
     `mode` line changes.
  3. A game folder given on the command line is never written.
  4. The F12 mode and the viewer scale are still saved.
  5. A failed write logs a `General` warning naming the path (test with an
     unwritable path).
  6. A new file (none existed) is written with the commented defaults as
     today, with V-11's comment corrected ("empty: `saves/` next to this
     file").
  7. Fuzz target of B-05 for the ini still passes its corpus.
- **Risk:** low. Line endings (CRLF files edited on Windows) must be kept:
  include a CRLF case in criterion 2.

### B-05 Fuzz targets for scripts, sound resources, patch resources and the ini

- **Kind:** requirement (safety net). **Severity:** Medium (G-48).
  **Effort:** M, 8 h. **When:** stage 5 step 0. **State:** Decided (Q-09).
- **Refs:** Q-09, G-48, R-138; `src/sci-vm/src/script.cpp`,
  `midi_parser.cpp`, `music.cpp`, `src/sci-audio/src/adlib.cpp` (patch
  bank) and the MT-32 patch resource, `config.cpp` of sci-platform
  (`src/app/platform/src/config.cpp` before B-02).
- **Acceptance criteria:**
  1. Four new libFuzzer targets `fuzz-script`, `fuzz-sound`,
     `fuzz-patch` (AdLib and MT-32 banks), `fuzz-config` in
     `tests/fuzz/`, each with a seed corpus made by hand or by a test-side
     builder (never game data; policy script passes).
  2. Each corpus replays as a CTest test in every preset, like the 12
     existing ones (`cmake/SciTesting.cmake:31-37`).
  3. `scripts/run-fuzzers.sh` and the CI `fuzz` job run all 16 targets
     (60 s each in the gate).
  4. Each target ran at least 10 minutes locally in the fuzz preset;
     every crash, leak or timeout found is a new defect B-43+ with its
     reproducer kept under `build/` (not committed if it contains a game
     byte; synthetic ones may be committed to the corpus only after the
     fix, in iteration 2).
  5. No source file outside `tests/fuzz/` and its CMake changes.
- **Risk:** low for behaviour (test-only). If a parser's entry point is
  private (`script.cpp` is under sci-vm `src/`), the target needs the
  private include path like `fuzz-save` does (`tests/fuzz/CMakeLists.txt:76`);
  B-23 records that exception.

### B-06 VM in CI: a test-side assembler and hand-built bytecode tests

- **Kind:** requirement. **Severity:** High (MAP 8.1: CI cannot detect a
  VM regression; sci-vm is 59 % of the code and `machine.cpp` 2.5 %
  covered in CI). **Effort:** L, 32 h. **When:** iteration 2, before any
  sci-vm refactoring beyond stage 5. **State:** Decided (Q-01).
- **Refs:** Q-01, MAP 6.4, 8.1, G-03, R-66.
- **Content:** a small assembler in `tests/support` (like `PicBuilder`)
  that writes SCI0 script and heap resources from readable opcodes; unit
  tests that run hand-built bytecode through `Machine::run_vm`: every
  opcode group (arithmetic incl. the signed/unsigned cases, branches,
  calls, sends, `super`, `self`, property access, stack ops, `rest`),
  object and class lookup, GC across calls, kernel dispatch with stub
  kernels, and the script patcher on assembled scripts.
- **Acceptance criteria:**
  1. Every SCI0 opcode the interpreter implements is executed by at least
     one CI test (a list in the test file, checked against the opcode
     table by a test).
  2. `machine.cpp` line coverage in the CI suite ≥ 70 % (method of MAP 6.4).
  3. The tests pass in all CI presets (Linux x64/arm64 release, asan,
     tsan; Windows) and under qemu-user.
  4. No game data in the tests; the assembler and its outputs are GPL
     first-party code.
- **Risk:** low for behaviour (tests only). The risk is false confidence:
  hand-built tests encode the author's reading of SCI0; each test names
  the ScummVM source it follows.

### B-07 VM in CI: a small home-made game

- **Kind:** requirement. **Severity:** High. **Effort:** L, 40 h.
  **When:** iteration 2. **State:** Decided (Q-01).
- **Refs:** Q-01, MAP 8.1, 6.4.
- **Content:** a synthetic SCI0 "game" built at test time by the B-06
  assembler and the existing builders: one or two rooms, one picture, a
  view with loops, a font, a cursor, a vocabulary, a menu, a text window,
  one sound, and scripts that walk an actor, open a window, parse input,
  play the sound, save and restore. Replays drive it through
  `run_deterministic` with goldens (hashes) of its traces and screens,
  committed because the data is ours.
- **Acceptance criteria:**
  1. Built and run in CTest in every preset, under 60 s per preset.
  2. Covers the kernel groups now at 0 % in CI (MAP 6.4: graphics,
     animate, menu, controls, strings, lists, movement, math, GC) with at
     least one call each.
  3. A save/restore round trip on it gives equal state hashes (as
     `run-round-trips.sh` does for the real game).
  4. Its goldens are generated twice with identical results.
  5. Together with B-06: sci-vm line coverage in CI ≥ 70 % (B-08).
- **Risk:** the game is a test asset that must itself be maintained;
  keep it small and generated, not a binary blob.

### B-08 sci-vm coverage gate in CI, and the local VM gate until it is met

- **Kind:** requirement. **Severity:** High. **Effort:** S, 4 h (the gate
  doc and script in stage 5; the CI coverage job in iteration 2, 3 h of
  the 4). **When:** stage 5 (local gate) / iteration 2 (CI job).
  **State:** Decided (Q-01 item 3).
- **Refs:** Q-01, MAP 6.4, B-06, B-07, B-19.
- **Acceptance criteria:**
  1. Stage 5: `docs/` (CONTRIBUTING or `tests/README.md`) states the gate:
     any change to `main` that touches `src/sci-vm`, `src/sci-resource`,
     `src/sci-text` or `src/sci-audio` passes `scripts/check-vm-hashes.sh`
     (B-19) on the customer's game locally before merge, with the log
     named in the pull request.
  2. Iteration 2: a CI job (Linux x64, gate only) builds the coverage
     configuration of MAP 6.4, prints per-module line coverage and fails
     when sci-vm is below 70 %; the job's cost is recorded under the CI
     cost policy (`docs/ASSUMPTIONS.md`).
  3. Once criterion 2 passes, the local gate stays required for releases
     only (written down).
- **Risk:** low; CI minutes (one more Linux job per gate, estimated
  10-15 min, not measured).

### B-09 No frame over 16.7 ms at N = 10

- **Kind:** performance. **Severity:** Medium (G-123; the one known miss:
  a whole frame at N = 10 takes 20.7 ms on room entry and mode switch,
  `reports/milestone-5.md:140-153`). **Effort:** M, 16 h (unknown until
  B-20's profile; could be L). **When:** iteration 2, only for misses
  B-20 measures. **State:** Decided (Q-04).
- **Refs:** Q-04, G-123, R-92, B-20, B-10.
- **Acceptance criteria:**
  1. With B-10's timing mode in the development container (4 cores, as
     the baseline), no frame of the 43-checkpoint playthrough and of a
     room-entry sweep over every room exceeds 16.7 ms at N = 10, room entry
     and F12 mode switch included (the analysis already cached, as today's
     baseline; first launch is a separate target, 15 s).
  2. Room entry ≤ 1 s at N = 10 and first launch ≤ 15 s still hold.
  3. Every B-18/B-19 hash is unchanged (optimisation must not change a
     pixel or a sample).
  4. Before/after numbers recorded in a report (median and worst of 5
     runs).
- **Risk:** medium-high. Moving room-entry work to another thread or
  frame (the likely fix, not measured) can change *when* a frame appears,
  which the scenario screens capture at fixed ticks: any trace or screen
  hash change stops the work. Determinism of parallel composition must be
  kept (`tests/threads`).

### B-10 Scripted timing mode and the customer's reference-laptop run

- **Kind:** requirement. **Severity:** Medium (G-123: never measured on
  reference-class hardware, Windows or ARM64). **Effort:** M, 6 h (the
  mode and its doc; the run itself is the customer's).
  **When:** iteration 2. **State:** Decided (Q-04).
- **Acceptance criteria:**
  1. `sierra-hd --timing <replay> --data <dir>` (or a tool sharing the
     program's presentation path, to be decided in iteration 2 by what
     measures the real presenter) plays a replay at full speed and writes
     a CSV: room entry per room, frame time percentiles and the worst
     frame, per N in {1, 2, 5, 10}, peak memory, first launch.
  2. A one-page instruction for the customer: which build, which replay,
     how to send the CSV.
  3. The customer runs it once on a 4-core x86-64 laptop of about 2020
     with integrated graphics, Windows (`docs/ASSUMPTIONS.md:854-857`);
     the result is recorded. A miss of 1 s room entry or 16.7 ms frames
     there raises B-09 to High (Q-04).
- **Risk:** low (a new mode; must not change normal runs: B-18/B-19).

### B-11 Peak memory at most baseline + 10 %

- **Kind:** performance. **Severity:** Low (no memory problem known; a
  regression limit). **Effort:** S, 3 h. **When:** iteration 2.
  **State:** Decided (Q-04).
- **Acceptance criteria:** B-20 records peak resident memory of the
  playthrough at N = 1 and 10 (Linux, `/usr/bin/time -v` or the B-10
  CSV); a local check script fails when a later run exceeds it by more
  than 10 %; the numbers are recorded per release.
- **Risk:** none for behaviour.

### B-12 Hint licensing section in `hints/README.md`

- **Kind:** requirement (documentation). **Severity:** High (G-24: the
  repository is public without the §15 licensing review recorded).
  **Effort:** S, 2 h. **When:** stage 5, plan step 4 (docs only).
  **State:** Decided (customer, 2026-11-06, Q-03 "1a 2b 3a").
- **Refs:** Q-03, G-24, `review/HISTORY_CLEANUP.md` E, C, D.
- **The customer's answer (2026-11-06, "1a 2b 3a"):** (1) the hints were
  made under the customer's personal Claude subscription; (2) the customer
  did not check whether those terms allow publishing the hints under
  GPL-3.0-or-later and takes that risk; (3) showing the customer's own
  game pictures to the model was allowed. The parser word files and the
  old `.pyc` stay in history (C, D; no rewrite).
- **Acceptance criteria:** `hints/README.md` gets a "Licensing" section
  saying exactly this:
  1. the hint files hold labels and geometry only, no image data
     (`review/HISTORY_CLEANUP.md` E);
  2. they were made with Claude under the customer's personal
     subscription;
  3. they are published under GPL-3.0-or-later at the customer's risk;
     the subscription's terms were not checked for this;
  4. the customer's own game pictures were shown to the model with the
     customer's permission.
  Plus a `docs/ASSUMPTIONS.md` entry pointing to it. Plan step 4 writes
  the section.
- **Risk:** none technical. Legal risk stays with the customer.

### B-13 `docs/ARCHITECTURE.md` brought up to date

- **Kind:** maintainability. **Severity:** Medium (G-44: stale since 4a,
  wrong for most of the code iteration 2 touches). **Effort:** M, 10 h.
  **When:** iteration 2, after B-02 (so its paths are final), before B-06.
  **State:** Decided (customer, 2026-11-06): accepted (batch-2 candidate; Q-05 made it
  a PART remainder).
- **Acceptance criteria:**
  1. Header states milestone 9 + iteration 2 and the date.
  2. Every module of MAP 1.1, incl. sci-platform in `src/sci-platform`,
     with its real dependency list (after B-21) and responsibilities
     (music player and saves in sci-vm: G-29).
  3. Data flows of MAP 3 (game files to resources, path 1, vector path, HD
     render to presentation, VM, audio, configuration/cache/saves with
     B-03's data folder) described as built, not "planned".
  4. Every statement checked against the code by a reviewer pass; MAP 8.4's
     five errors gone.
- **Risk:** none for behaviour.

### B-14 Other stale docs, comments and missing READMEs

- **Kind:** maintainability. **Severity:** Low. **Effort:** M, 6 h.
  **When:** iteration 2 (comment-only edits in code are allowed there; in
  stage 5 only the path references B-01/B-02 touch are edited).
  **State:** Decided (customer, 2026-11-06): accepted.
- **Refs and the list:** G-155, G-156, G-39, G-126; MAP 8.5, 8.6, 8.13;
  V-03, V-05, V-07, V-11:
  - `src/sci-vm/README.md:7`, `:11` (silent music, saves failing);
  - `src/sci-vector/include/sci/vector/hints.hpp:78-80` and
    `hints/README.md` (relations "used from milestone 3b"; with B-36);
  - `cmake/SciThirdParty.cmake:5-6` (SDL3 "audio output of sci-audio");
  - `docker/run.sh:8` (`--preset linux-debug` does not exist);
  - `docs/ASSUMPTIONS.md:28` ("private"; a new entry, not an edit);
  - `config.cpp:184` (V-11; the code fix is B-04);
  - `trigonometry.hpp:3-7` (V-05; with B-39);
  - `docs/formats/replay-and-trace.md:40` (V-07: the program's `--replay`
    keeps host input);
  - `fitting.cpp:15-17` (V-03; with B-29);
  - a README for `src/sci-platform` (G-39), `tools/common`,
    `tools/graphics-dump`, `tools/resource-dump`, `tools/playthrough`
    (MAP 8.13; the last may point to `docs/playthrough.md`);
  - `README.md`: configuration options or a pointer to
    `docs/USER-README.md` (G-126), no claim of published binaries until
    B-16 publishes.
- **Acceptance criteria:** each line above fixed or struck with a reason;
  `git grep` for the quoted stale phrases finds nothing; only comments and
  docs change (B-18/B-19 hashes unchanged).
- **Risk:** none.

### B-15 Policy script and `.gitignore`: SCI0 patch files and run-time folders

- **Kind:** defect (a guard that misses cases). **Severity:** Medium
  (G-18, V-06). **Effort:** S, 2 h. **When:** stage 5. **State:** Decided
  (G-18 folded into Q-07; the customer's rule "never commit game data").
- **Refs:** G-18, V-06, R-135, B-02.
- **Facts:** `scripts/check-repo-policy.py` `GAME_DATA` and `.gitignore`
  do not match SCI0 patch files `TYPE.NNN` (`SCRIPT.052`, `TEXT.052`,
  `PATCH.001`, `VIEW.123`, …), which the GOG release ships. `.gitignore`
  also ignores `analysis-cache/` while the program writes `cache/`, and
  does not ignore `files/`, `saves/` or `*.sav` (`main.cpp:1125`, `:1137`,
  `config.cpp:194`; found in stage 4). Nothing of that kind is tracked
  today (V-06 grep).
- **Acceptance criteria:**
  1. Both match (case-insensitive) a whole file name
     `(^|/)(view|pic|script|text|sound|memory|vocab|font|cursor|patch)\.\d{3}$`
     (the SCI0 resource types) anywhere in the tree except `third_party/`;
     the name must end after the number, so `hints/pic.052.json` stays
     allowed.
  2. `*.sav` and `sierra-hd.ini` are ignored everywhere; `cache/`,
     `files/`, `saves/` under `local/` (B-02) are ignored by its rule.
  3. A self-test (`scripts/check-repo-policy.py --self-test`, or a CTest
     test) feeds a list of names, including `SCRIPT.052`, `text.052`,
     `PATCH.001` and `local/game/RESOURCE.MAP`, and expects each rejected;
     and a list of legitimate names (`tests/scenarios/*.txt`,
     `hints/pic.052.json`) expects each accepted.
  4. The policy script passes on the tree.
- **Risk:** a pattern too wide flags a legitimate file (criterion 3's
  accept list).

### B-16 Release workflow kept working without macOS; publishing by the customer

- **Kind:** requirement. **Severity:** Medium (G-125 was High; the
  customer decided "we'll publish releases later", so the open part is
  keeping the workflow able to publish). **Effort:** S, 3 h. **When:**
  macOS removal in stage 5 (B-01); the rest iteration 2. **State:**
  Decided.
- **Refs:** G-125, R-141, B-01, B-03, B-12.
- **Acceptance criteria:**
  1. `release.yml` keeps the `push: tags: v*` trigger, the Linux x64/arm64
     and Windows x64/arm64 jobs with tests, `package-release.sh` and the
     `publish` job (`needs: [linux, windows]`).
  2. A manual dispatch on `main` (no tag) builds, tests and uploads all
     four archives as workflow artifacts and skips `publish`
     (`if: github.event_name == 'push'`, today's condition); this run is
     done once in iteration 2 and its run URL recorded.
  3. The archives contain the program, `hints/`, `README.md`, licences and
     the empty `game/`, `rom/`, `saves/` (B-03); a check lists the archive
     and finds no game file (the B-15 patterns).
  4. `docs/release-notes/<tag>.md` exists for the next tag before it is
     pushed (`publish` reads it, `release.yml` `--notes-file`).
  5. `README.md` says binaries are published on GitHub releases only once
     one exists; until then it says how to build.
  6. The customer pushes the tag (the session cannot push tags; the local
     tags `v0.1.0`, `v0.9.0-rc.1` are the customer's to push or drop).
- **Risk:** low. A tag pushed before B-12 is confirmed publishes the hint
  files under the ASSUMED licensing.

### B-17 `kDoAvoider` stub

- **Kind:** defect (latent). **Severity:** Low (R-65, R-66: the tested
  release does not reference it, `docs/ASSUMPTIONS.md:1205-1208`).
  **Effort:** S, 1 h (the scan, when a copy exists); porting from ScummVM
  `engines/sci/engine/kmovement.cpp:401` would be M, 6 h, only if a scan
  finds a caller. **When:** iteration 2, conditional. **State:**
  Decided (customer, 2026-11-06): accepted (not asked; batch-1 notes).
- **Acceptance criteria:** if another accepted release turns up,
  `sci-vm-trace --scan-kernel` on it is run and recorded; DoAvoider absent
  → item closed; present → port it with a CI bytecode test (B-06).
- **Risk:** none until a copy turns up.

## Safety net for stage 5 (decided in batch 1)

These three items are stage 5 step 0 of `review/REFACTOR_PLAN.md`. They add
test tooling and committed hash files only; they change no program
behaviour. They stay useful after stage 5 as the local gate (B-08).

### B-18 Golden outputs for all resources

- **Kind:** maintainability (safety net). **Severity:** High (MAP 8.3,
  V-04: the pixel goldens cover pictures only; views, fonts, cursors, the
  HD priority map, N other than 1, 2, 5, 10 and composed frames have none).
  **Effort:** M, 16 h, plus the generation run time (not measured).
  **When:** stage 5 step 0. **State:** Decided (Q-02).
- **Refs:** Q-02, V-04, R-56, R-57, G-54, G-148, MAP 6.3, 8.3.
- **Scope (Q-02 default, decided):**
  - every picture: 1x visual, priority and control maps; HD image and HD
    priority map at every N = 1…16;
  - every view loop and cel, every font glyph and every cursor at 1x and
    through the HD sprite path at N = 1, 2, 5, 10;
  - every screen of the 39 scenarios and of the 43 playthrough
    checkpoints, original and HD at N = 1, 2, 5, 10;
  - sounds: the existing 119 OPL hashes plus the MT-32 MIDI streams.
- **Acceptance criteria:**
  1. `sci-golden-hashes` (extended) or a new local tool writes one SHA-256
     per image or stream into committed files under `tests/golden/`
     (hashes only, game-free); PNGs at N = 1, 2, 5, 10 only under `build/`.
  2. Generated twice from two clean builds of the original code; the two
     files are byte-identical (determinism proven).
  3. A `--check` mode reports every mismatch by resource, N and layer, and
     a diff helper writes a pixel-difference image under `build/` for a
     mismatch (regenerating the reference from a kept build of the
     original code for N not kept as PNG).
  4. Run on x86-64 after every stage 5 commit; on ARM64 (qemu-user) once
     at the start and once at the end.
  5. The existing `tests/golden/pictures.txt` and `sounds.txt` are a
     subset and still match.
- **Risk:** the tool itself is new code: a bug that hashes the wrong
  buffer would pass everything. Mitigation: a CI test on synthetic
  resources checks that a one-pixel change changes the hash for every
  layer kind.

### B-19 Hashes of every VM output

- **Kind:** maintainability (safety net). **Severity:** High (MAP 8.1:
  the only net for sci-vm). **Effort:** M, 10 h. **When:** stage 5 step 0.
  **State:** Decided (Q-01).
- **Refs:** Q-01, MAP 6.3, 6.4, R-66.
- **Acceptance criteria:**
  1. `scripts/check-vm-hashes.sh` (new, local) runs `sci-vm-trace` in
     deterministic mode over the 39 scenarios (AdLib and, with the
     customer's ROMs, MT-32), the 43-checkpoint playthrough and the save
     round trips, and records a SHA-256 per output stream per run (kernel
     trace, screens, 1x maps, actors, parser, layout, OPL, MIDI, state
     hashes, end state) in a committed `tests/golden/vm.txt`. Hashes are
     taken of the files `sci-vm-trace` already writes (`sha256sum`), so the
     tool needs no change; a hash-only option is added only if disk space
     forces it (test tooling, no program change).
  2. Generated twice; identical.
  3. `--check` compares and names the first differing stream; the full
     traces of the original code stay under `build/vm-baseline/` to diff.
  4. The program playthrough under Xvfb (`tools/playthrough/program-run.sh`)
     end state is included.
  5. ScummVM comparison (`run-all-scenarios.sh`, `tools/playthrough/check.sh`)
     run once at the start and once at the end of stage 5, both logs kept.
  6. Run time of one full check recorded (needed for B-08's gate and to
     plan the per-commit cost).
- **Risk:** as B-18 (tooling bugs); plus disk: full traces of the
  playthrough are large (not measured).

### B-20 Baselines: warnings, sanitizers, timings, memory, size, build time

- **Kind:** maintainability (safety net). **Severity:** Medium. **Effort:**
  M, 6 h. **When:** stage 5 step 0. **State:** Decided (Q-04; brief 5.1).
- **Acceptance criteria:**
  1. Warnings: a clean build of every Linux preset with
     `SCI_WARNINGS_AS_ERRORS=OFF` and `-Wall -Wextra` (already in the
     project flags, `cmake/SciTargets.cmake:20-39`); count and list of
     warnings per preset committed in `review/` (stage 5 report) — expected
     0 (MAP 6.4 build had 0).
  2. Sanitizers: `asan` and `tsan` workflows, plus the local game checks
     (B-19's playthrough and scenarios) under the `asan` build; results
     recorded.
  3. Timings: room entry per room at N = 1, 2, 5, 10; first launch; frame
     time percentiles and worst frame; median of 5 runs, in the
     development container (`sci-hd-frames --timing`).
  4. Peak memory (B-11), binary size (stripped and not), clean build time
     per preset, CI minutes of the last gate (`scripts/ci-minutes.py`).
  5. All CTest tests and every B-18/B-19 check pass on the original code
     before step 1.
- **Risk:** none (measurement).

## Improvements

### B-21 Remove the four unused dependency edges

- **Kind:** architecture. **Severity:** Low (G-31; MAP 8.7). **Effort:**
  S, 1 h. **When:** stage 5. **State:** Decided (customer, 2026-11-06): accepted
  (batch-2 candidate G-31); in the plan.
- **What:** delete `sci-resource` from the `DEPENDS` of sci-pic, sci-view
  and sci-text, and `sci-view` from sci-render (MAP 1.3: no include, no
  `resource::` or `view::` use).
- **Acceptance criteria:** all presets build (a missing transitive include
  fails at compile time); CTest passes; B-18/B-19 unchanged; MAP 1.3's
  grep confirms no use.
- **Risk:** low. A consumer that relied on a transitive include path breaks
  the build, never the behaviour. Link order of static libraries changes,
  which cannot change behaviour without duplicate symbols (the build would
  report them).

### B-22 Lint check that every declared edge is used

- **Kind:** maintainability. **Severity:** Low. **Effort:** S, 3 h.
  **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Acceptance criteria:** a script (in the lint job) parses each
  `sci_add_library(... DEPENDS ...)` and fails when a library includes no
  header of a declared dependency, or includes one it did not declare;
  passes on the tree after B-21.
- **Risk:** none for behaviour.

### B-23 Tests that include private `src/` folders

- **Kind:** architecture. **Severity:** Low (MAP 1.4, 8.9; it matters when
  iteration 2 refactors sci-vm and sci-vector internals). **Effort:** S,
  2 h in stage 5; M, 12 h in iteration 2. **When:** both.
  **State:** stage 5 part in the plan; iteration-2 part
  Decided (customer, 2026-11-06): accepted.
- **Stage 5 (behaviour-neutral):** replace the five ad-hoc
  `target_include_directories(... src)` lines
  (`tests/unit/CMakeLists.txt:49`, `:89`, `:98`, `:110`,
  `tests/fuzz/CMakeLists.txt:76`) by one helper
  `sci_test_private_includes(<test> <library>)` in `cmake/SciTesting.cmake`
  that documents the exception and lists the users; B-05's targets use it.
  No test source changes.
- **Iteration 2:** tests of sci-vector reach only the public interface;
  sci-vm tests keep private access through the helper (the VM's internals
  are what B-06 tests), documented as the one allowed exception.
- **Acceptance criteria:** the private paths appear only in the helper;
  same tests, same results; B-18/B-19 unchanged.
- **Risk:** none for behaviour (build-only).

### B-24 Remove the dead `component_name()` placeholders

- **Kind:** tech debt. **Severity:** Low (G-43, MAP 8.8). **Effort:** S,
  0.5 h. **When:** stage 5. **State:** Decided (customer, 2026-11-06): accepted; in the
  plan.
- **What:** delete `src/sci-audio/src/component.cpp`,
  `src/sci-vm/src/component.cpp` and their declarations; no caller (`grep
  -rn component_name src tools tests`: definitions and headers only).
- **Acceptance criteria:** the grep finds nothing; builds and CTest pass;
  B-18/B-19 unchanged.
- **Risk:** none (the linker would report a caller).

### B-25 Check the vendored files' SHA-256

- **Kind:** maintainability (supply chain). **Severity:** Medium (MAP 5,
  8.11: the per-file SHA-256s in `THIRD_PARTY.md` and the `VENDORED.md`
  files are not checked by anything; whether the tree still matches them
  was not verified). **Effort:** S, 3 h. **When:** stage 5. **State:** in
  the plan.
- **Acceptance criteria:**
  1. `scripts/check-vendored.py` recomputes every recorded SHA-256 and
     fails on a mismatch, a missing file, or a file under `third_party/`
     with no record (or a recorded whole-tree hash, if per-file records do
     not exist for SDL3's 1,511 files; to be seen in the step).
  2. Run in the lint job and in `scripts/pre-push.sh`.
  3. First run on `main`: any mismatch is logged as a defect B-43+ and
     **not fixed** in stage 5.
- **Risk:** none for behaviour.

### B-26 SPDX headers on scripts; the policy checks `.py` and `.sh`

- **Kind:** maintainability. **Severity:** Low (MAP 8.10: 15 tracked
  Python/shell files in `scripts/` and `tools/` have none; a stage 4 count
  over the whole tree outside `third_party/` finds 19: also 3 in `docker/`
  and 1 in `tests/`). **Effort:** S,
  1.5 h. **When:** stage 5. **State:** in the plan.
- **Acceptance criteria:** every tracked `.py`/`.sh` outside `third_party/`
  has `SPDX-License-Identifier: GPL-3.0-or-later` in its first three lines
  (after the shebang); `SOURCE` in `scripts/check-repo-policy.py` covers
  `.py` and `.sh` and the script passes; scripts produce the same output
  (a comment line only).
- **Risk:** none.

### B-27 A reason on every `NOLINT`

- **Kind:** maintainability. **Severity:** Low (G-36: 20 of 137 without a
  reason). **Effort:** S, 2 h. **When:** iteration 2. **State:**
  Decided (customer, 2026-11-06): accepted (batch-2 candidate).
- **Default:** add the reason to each; a lint rule rejects a bare
  `NOLINT`. The macOS-only reason at `build_scene.cpp:499` (B-01) is
  re-examined: with macOS gone, `std::ranges::iota` may replace the
  suppressed code (a code change, golden-checked).
- **Acceptance criteria:** the G-36 grep finds nothing; lint fails on a new
  bare `NOLINT`.
- **Risk:** none (comments), except the optional `iota` change (B-18).

### B-28 Switchable log categories in the program

- **Kind:** requirement. **Severity:** Low (G-41, V-10, R-121: `Resources`
  and `Vm` warnings can never be seen in the program). **Effort:** S, 4 h.
  **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted
  (batch-2 candidate).
- **Default:** `--log vm,resources` and an ini key `log =`; off by default;
  release builds keep them (§5.1).
- **Acceptance criteria:** with `--log vm`, a script workaround warning
  appears on stderr (tested with B-07's mini-game); without it, output is
  unchanged.
- **Risk:** logging on the hot path costs time; criterion: no B-20 timing
  change with logging off.

### B-29 The range of N

- **Kind:** requirement. **Severity:** Low (G-148, V-03: N goes to 16, the
  goldens and targets stop at 10, a comment assumes 10). **Effort:** S,
  2 h. **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted
  (batch-2 candidate).
- **Default:** keep N ≤ 16 (no behaviour change); B-18 already covers
  N = 11-16 in the goldens; recompute the flattening error at N = 16 for
  `fitting.cpp:15-17` and correct the comment; state in `docs/` that the
  §10.3 targets apply up to N = 10 only.
- **Alternative:** cap at 10: a behaviour change (`config.hpp:36`,
  `compose.hpp:94`), users with `scale = 16` get 10.
- **Acceptance criteria (default):** the comment states the computed error
  at N = 16; docs name the range and where targets apply.
- **Risk:** none (default).

### B-30 Other sound devices

- **Kind:** requirement. **Severity:** Low (G-145). **Effort:** none with
  the default. **State:** Decided (customer, 2026-11-06): accepted: **no other device**
  (AdLib and MT-32 only, as built). If the customer wants one (e.g. PC
  speaker, General MIDI out), it becomes a new item with its own estimate.

### B-31 Hint files for pictures 55 and 992

- **Kind:** maintainability. **Severity:** Low (G-79). **Effort:** S,
  0.5 h. **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Default:** no files (the two pictures need no hints,
  `docs/ASSUMPTIONS.md:909`); `hints/README.md` says "one file per picture
  that needs hints" and names the two.
- **Acceptance criteria:** the README names both and why; the hint check
  passes.
- **Risk:** none.

### B-32 Make the application layer testable

- **Kind:** tech debt. **Severity:** Medium (MAP 8.2: `main.cpp` 1,207
  lines — options, `GameHost`, presenter, event loop, audio opening, end
  state — and `viewer.cpp`, `file_store.cpp`, `hd_pictures.cpp`,
  `game_files.cpp` are in no test binary; R-110 untested; B-03, B-04 and
  B-10 all change this code). **Effort:** M, 14 h. **When:** iteration 2,
  before B-03. **State:** Decided (customer, 2026-11-06): accepted.
- **Default:** move `parse_options`, the data-folder resolution (B-03) and
  the ini persistence (B-04) into sci-platform functions with CTest tests
  (help, exit codes, invalid combinations of R-110); keep SDL code in app.
- **Acceptance criteria:** R-110's rules each have a test; `main.cpp`
  shrinks by the moved code; B-18/B-19 and the program playthrough
  unchanged.
- **Risk:** medium: the event loop and presenter are timing-sensitive; the
  default moves only pure functions.

### B-33 `-fwrapv` only in some build types

- **Kind:** tech debt. **Severity:** Low (MAP 8.12: Release and
  RelWithDebInfo without sanitizers get `-fwrapv`; Debug and sanitizer
  builds do not, `cmake/SciCompilerFlags.cmake:91-96`). The goldens and
  the VM hashes run on Release, so a reliance on wrapping that UBSan would
  flag might hide there. **Effort:** S, 3 h. **When:** iteration 2 (a
  flag change can change code generation). **State:**
  Decided (customer, 2026-11-06): accepted.
- **Default:** B-20's UBSan run over the local game checks shows whether
  any signed overflow happens; if none, keep as is and document why; if
  some, a defect item per site.
- **Acceptance criteria:** the decision and its evidence recorded in
  `docs/ASSUMPTIONS.md`.
- **Risk:** none (investigation).

### B-34 Test hooks in the shipped program

- **Kind:** requirement. **Severity:** Low (G-151 CREEP: `--replay`,
  `--seed`, `--fast`, `--end-state`). **Effort:** S, 1 h. **When:**
  iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Default:** keep them (the program playthrough and B-10 need them);
  document them in `docs/USER-README.md` as testing options.
- **Risk:** none.

### B-35 Remove `SCI_WITHOUT_STROKE_INTEGRITY`

- **Kind:** tech debt. **Severity:** Low (G-153: a second code path that
  no preset builds, kept from a 3b measurement). **Effort:** S, 1 h.
  **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Acceptance criteria:** the option and its `#if` branch
  (`build_scene.cpp:1257-1261`) are gone; default-build output unchanged
  (B-18).
- **Risk:** none for the default build.

### B-36 Remove relation-hint parsing; fix its comments

- **Kind:** tech debt. **Severity:** Low (G-154, G-97 DROP, V-02).
  **Effort:** S, 2 h. **When:** iteration 2. **State:**
  Decided (customer, 2026-11-06): accepted.
- **Default:** relations are dropped (answer 3 after stage 1b), so their
  parsing and validation go, and `hints.hpp:78-80` and `hints/README.md`
  stop claiming they are used; region and symmetry hints stay (spec
  content, §6.7) and are documented as informational. A hint file that
  still has a `relations` key is rejected by the schema (none has one).
- **Acceptance criteria:** schema and parser agree; `fuzz-hints` corpus
  passes; B-18 unchanged.
- **Risk:** low.

### B-37 List thin fallbacks in the replacement report

- **Kind:** requirement (tool). **Severity:** Low (G-69). **Effort:** S,
  2 h. **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Acceptance criteria:** `sci-hd-report` lists every fallback block,
  thin faces included (§6.4), with a count per picture.
- **Risk:** none (tool only).

### B-38 Check the `analyser_version` rule

- **Kind:** maintainability. **Severity:** Low (V-12). **Effort:** S, 2 h.
  **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Acceptance criteria:** a lint step fails when a change to
  `tests/golden/pictures.txt` (or B-18's picture files) has no matching
  bump of `analyser_version` (`build_scene.hpp:97-99`).
- **Risk:** none.

### B-39 Trigonometry outside -360…360

- **Kind:** defect (unverified). **Severity:** Low (V-05: the table is used
  modulo 360 and "may differ from glibc's in the last bits", so from
  ScummVM; whether the game passes such angles is not known). **Effort:**
  S, 2 h. **When:** iteration 2. **State:** Decided (customer, 2026-11-06): accepted.
- **Default:** count, in B-19's playthrough and scenarios, the kernel
  calls with angles outside the range (trace grep); none → fix only the
  comment (B-14); some → compare with glibc and decide.
- **Risk:** none (investigation).

### B-40 5.25" release hint warning

- **Kind:** defect (unverified). **Severity:** Low (V-01, V-14; R-06).
  **State:** Decided (customer, 2026-11-06): struck: the customer has no 5.25" copy and its
  acceptance is untested scope (answer 2 after stage 1b). Reopen if a copy
  turns up.

### B-41 LGPL static-linking note

- **Kind:** maintainability. **Severity:** Low (V-09, a hypothesis, not a
  legal opinion). **State:** Decided (customer, 2026-11-06): struck: out of the code's scope;
  the customer decides whether to ask counsel. If kept: one paragraph in
  `THIRD_PARTY.md` on how users can relink (source is published under the
  GPL).

### B-42 `uint32_t` sums instead of `uint64_t`

- **Kind:** tech debt. **Severity:** Low (G-73: a letter-only deviation; no
  overflow possible, 33,553,920 < 2^32). **State:** Decided (customer, 2026-11-06): struck;
  recorded as an accepted deviation in `docs/ASSUMPTIONS.md` (B-14).
  Changing the type would touch the rasteriser for no visible gain.

## Gap items closed without a backlog item

| G / item | Why no B-item |
|---|---|
| G-07 | → B-01 (macOS dropped) |
| G-18 | → B-15, B-02 |
| G-24 | → B-12 (Decided, Q-03) |
| G-29 | → B-02 (sci-platform gets its own folder), B-13 (docs) |
| G-34 | decided and reasoned (audio mixing on the main thread) |
| G-44, G-155, G-156, G-39, G-126 | → B-13, B-14 |
| G-48 | → B-05 |
| G-69 | → B-37 |
| G-77, G-91, G-102, G-115, G-142 | declined by the customer |
| G-87, G-95, G-96 | HD quality frozen at milestone 9 (Q-08) |
| G-97, G-98 | dropped (answer 4); parsing leftovers → B-36 |
| G-103 | no grid tooling: stage 5 changes no pixel (Q-05 b) |
| G-122 | → B-03 |
| G-123 | → B-09, B-10, B-11, B-20 |
| G-125 | → B-16; publishing is the customer's step |
| G-145 | → B-30 |
| G-146 | closed (customer, 2026-11-06: patches stay) |
| G-147, G-149 | resolved by recorded definitions and the mapping table |
| G-148 | → B-29 |
| G-150 | closed by Q-05 |
| G-151, G-152, G-153, G-154 | → B-34, (G-152 kept: harmless conveniences, no item), B-35, B-36 |
| MAP 8.1 / G-03 | → B-06, B-07, B-08, B-19 |
| R-65 / R-66 | → B-17 |

## Summary

### Counts by kind and severity

Counted by a script over the 42 rows of the overview table (B-01 … B-42;
a kind in brackets, e.g. "requirement (defect)", counts under its first
word).

| Kind | Critical | High | Medium | Low | Total |
|---|---|---|---|---|---|
| requirement | 0 | 5 (B-01, B-06, B-07, B-08, B-12) | 6 (B-02, B-03, B-04, B-05, B-10, B-16) | 5 (B-28, B-29, B-30, B-34, B-37) | **16** |
| defect | 0 | 0 | 1 (B-15) | 3 (B-17, B-39, B-40) | **4** |
| tech debt | 0 | 0 | 1 (B-32) | 5 (B-24, B-33, B-35, B-36, B-42) | **6** |
| architecture | 0 | 0 | 0 | 2 (B-21, B-23) | **2** |
| performance | 0 | 0 | 1 (B-09) | 1 (B-11) | **2** |
| maintainability | 0 | 2 (B-18, B-19) | 3 (B-13, B-20, B-25) | 7 (B-14, B-22, B-26, B-27, B-31, B-38, B-41) | **12** |
| **Total** | **0** | **7** | **12** | **23** | **42** |

By state (updated 2026-11-06 after the customer's "2 all approved" and
Q-03 "1a 2b 3a"): Decided 40 — 17 by earlier answers (B-01 … B-12,
B-15, B-16, B-18, B-19, B-20), 20 defaults accepted (B-13, B-14, B-17,
B-21, B-22, B-23 iteration-2 part, B-24, B-27 … B-39), 3 struck (B-40,
B-41, B-42); in the plan without a separate decision 2 (B-25, B-26, both
behaviour-free hygiene the brief's "significantly reduce risk" covers);
ASSUMED 0; open 0. (Before: Decided 16, ASSUMED 1 (B-12), default — accept
or strike 20, default — strike 3.)

By timing: stage 5 (all or part) 15 — B-01, B-02, B-05, B-08 (local
gate), B-12, B-15, B-16 (macOS part), B-18, B-19, B-20, B-21, B-23 (helper),
B-24, B-25, B-26; iteration 2 the rest.

Effort of the iteration-2 work with every default accepted (sum of the
estimates of B-03, B-04, B-06, B-07, B-08 CI part, B-09, B-10, B-11, B-13,
B-14, B-16 rest, B-17 scan, B-22, B-23 rest, B-27 … B-39): about 190 hours,
of which the VM net (B-06, B-07) is 72. Judgement, not measured.

### Top points for the customer

1. **The VM net is the biggest item and the biggest risk** (B-06, B-07,
   B-08, High, L): until CI covers sci-vm, every sci-vm change in
   iteration 2 needs the local gate (B-19 hashes on your game).
2. **Stage 5 gets the decided safety net first** (B-18, B-19, B-20, B-05):
   about 40 hours before the first refactoring commit.
3. **The layout move (B-02) and the macOS removal (B-01) are done in stage
   5**, as behaviour-free steps, so iteration 2 starts on the final tree.
4. **Hint licensing: decided 2026-11-06** (B-12, Q-03 "1a 2b 3a"). Was: stays ASSUMED (B-12, High): no release should be
   tagged until you confirm it.
5. **23 defaults: all approved 2026-11-06** ("2 all approved"). Was: 23 defaults to accept or strike (batch-2 candidates and PART
   remainders): silence means accept; B-40, B-41, B-42 default to strike.
