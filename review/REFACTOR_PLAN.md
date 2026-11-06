# Stage 4: interim refactoring plan (for stage 5)

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-06.
Read-only: nothing outside `review/` was changed. **This plan is not
started.** Stage 5 begins only after the customer approves it.

The brief: "Interim refactoring plan, limited to changes that block
iteration 2 or significantly reduce risk. Behavior must not change." The
items are in `review/BACKLOG_V2.md` (B-xx). This plan covers stage 5 only,
not the whole backlog.

## Scope in one paragraph

Stage 5 builds the safety net the customer decided (Q-01, Q-02, Q-04,
Q-09), proves it deterministic on the original code, and then makes eleven
small, behaviour-free changes: script and policy hygiene, the vendored
checksum check, the hint licensing text, the macOS removal, two dead-code
and dependency clean-ups, one test-build helper, and the repository layout
move. **No sci-vm, sci-render, sci-vector or app logic is restructured in
stage 5**: none of that blocks iteration 2, and the VM's CI net (B-06,
B-07) does not exist yet, so restructuring the VM now would rest on local
checks alone.

## Branch and working rules

- **Branch:** `refactor`, created from `a0422d6` (`origin/main`;
  `git switch -c refactor a0422d6`). Note: the local branch `main` in this
  clone is stale at `df81541`, the first commit, so `main` must not be
  used as the start point by name. Its first
  commit brings in `review/` from `audit` (`git checkout audit -- review`;
  `audit` differs from `main` only in `review/`), so `REFACTOR_REPORT.md`
  and new stage 5 bugs (B-43+) are written on the same branch as the code.
  Nothing is pushed or merged without the customer's instruction.
- **One step = one commit** (step 0 has one commit per sub-step), message
  "Stage 5 step N: …", each naming its B-item.
- **The full suite runs after every commit** (defined below). A commit
  that fails any part is reverted at once (`git revert`), the cause logged;
  no fix-forward.
- **Prohibited** (brief 5.3): changing observable behaviour, fixing bugs
  "while at it", going beyond this plan. A bug found is logged as B-43+ in
  `review/BACKLOG_V2.md` with the step that found it, and left alone.

## The full suite (after every commit)

| Part | What | Where | Source |
|---|---|---|---|
| S1 | Lint: clang-format, policy script, hint check, clang-tidy (debug) | container | `scripts/pre-push.sh` first part |
| S2 | `cmake --workflow` for `release`, `asan` (ASan+UBSan), `tsan`: all CTest tests | container | MAP 4 |
| S3 | Fuzz corpus replays (16 targets after step 0.5) | container | part of S2 |
| S4 | Golden outputs of all resources, `--check`, x86-64 release build | container + customer's game | B-18 |
| S5 | VM output hashes, `--check` (39 scenarios, playthrough, round trips, program run under Xvfb) | container + game (+ MT-32 ROMs if present) | B-19 |
| S6 | Build identity: `compile_commands.json` and `ninja -t commands` diff against the previous commit, and the `sierra-hd` binary's SHA-256 if step 0.1 shows builds are reproducible | container | this plan |

Not per commit (once at the start and once at the end): `cross-aarch64`
(ARM64 under qemu-user) with S4; Windows presets (CI gate run on a draft
pull request, if the customer allows CI minutes; otherwise once at the
end); the ScummVM comparisons (`run-all-scenarios.sh`,
`tools/playthrough/check.sh`); fuzzing for time (10 min per target at the
end, 60 s in the CI gate).

The time of one S1-S6 run is measured in step 0 (B-19 criterion 6). If it
is much over an hour, the customer is asked before step 1 whether S5 may
run per step instead of per commit (each step is a single commit anyway,
so the difference is only inside step 0).

## How "behaviour unchanged" is shown, per kind of change

| Change touches | Proof |
|---|---|
| Only docs, comments in scripts, CI YAML, `.gitignore`, policy script | S6: `sierra-hd` and every tool binary byte-identical (if reproducible), compile commands identical; S1-S3 pass |
| CMake only (no source) | S6: compile and link commands identical on Linux, or differing only in the expected flags/paths (listed in the commit message); S2-S5 |
| C++ deletions (dead code) | S6: only the deleted translation units vanish from the command list; S2-S5 |
| File moves | S6: commands identical after replacing the old path by the new; S2-S5; `git log --follow` on a moved file |

The program's observable behaviour is its screen output, sound output,
files written (ini, saves, cache, game files), exit codes and console
messages. S4 covers screens and sound per resource, S5 the game's run (VM
traces, screens, sound, state, saves, end state of the real program).
Assertion messages carry the source file path (`std::source_location`,
`src/sci-core/include/sci/core/assert.hpp:27`); the path change in step 9
alters those strings only, which appear only when an assertion fails.

## Step 0: the safety net (no change to `src/`)

Order fixed; each sub-step is one commit; at the end, all tests pass on
the original code, and every golden is generated twice and identical.

| Sub-step | What changes | Why (B) | How behaviour is shown unchanged | Effort | Rollback |
|---|---|---|---|---|---|
| 0.1 | Create `refactor`; bring in `review/`; build the original code twice in clean trees (`build/baseline-release`, `build/baseline-release-2`), compare `sierra-hd` and tool binaries (reproducibility recorded); keep `build/baseline-release` as the reference build for the whole stage | B-18, B-19 | `git diff main -- . ':!review'` empty | S, 1 h + build time | delete the branch |
| 0.2 | Baselines (B-20): warning count per preset (`-Wall -Wextra` and the project's set, `SCI_WARNINGS_AS_ERRORS=OFF`); ASan/UBSan and TSan workflows; UBSan over the local game checks; timings (room entry per room at N = 1, 2, 5, 10; first launch; frame percentiles and worst frame; median of 5); peak memory; binary size stripped and unstripped; clean build time; CI minutes of the last gate. Recorded in `review/` (stage 5 report draft); no code | B-20 (Q-04) | no source change | M, 6 h | revert |
| 0.3 | Golden outputs of all resources (B-18): extend `tools/golden-hashes` or add a local tool; committed hash files under `tests/golden/`; a CI test on synthetic resources that a one-pixel change changes each layer's hash; PNGs for N = 1, 2, 5, 10 under `build/` only. Scope exactly as Q-02: every picture (1x visual, priority, control; HD image and HD priority map at N = 1…16), every view cel, font glyph and cursor at 1x and N = 1, 2, 5, 10, every scenario and checkpoint screen (original and HD at N = 1, 2, 5, 10), the 119 OPL hashes and the MT-32 MIDI streams. **Generated twice from the two builds of 0.1; byte-identical** | B-18 (Q-02) | `src/` untouched; the existing `tests/golden/pictures.txt` and `sounds.txt` still match | M, 16 h + run time | revert; nothing depends on it yet |
| 0.4 | VM output hashes (B-19): `scripts/check-vm-hashes.sh`, committed `tests/golden/vm.txt` (hashes of the files `sci-vm-trace` writes for the 39 scenarios, the playthrough, the round trips, plus the program run's end state); full traces kept under `build/vm-baseline/`. **Generated twice; identical.** ScummVM comparison run once (start), log kept | B-19 (Q-01) | `src/` untouched | M, 10 h + run time | revert |
| 0.5 | Four fuzz targets (B-05): `fuzz-script`, `fuzz-sound`, `fuzz-patch`, `fuzz-config`, synthetic seed corpora, CTest corpus replays, wired into `run-fuzzers.sh` and the CI `fuzz` job; 10 min each locally. Crashes → B-43+, not fixed | B-05 (Q-09) | only `tests/fuzz/`, `scripts/run-fuzzers.sh`, `ci.yml` change; S6 shows `sierra-hd` unchanged | M, 8 h | revert |
| 0.6 | The local VM gate written down (B-08 stage 5 part): `tests/README.md` (or `CONTRIBUTING.md`) says which changes need S4/S5 before merging, and `scripts/check-local.sh` runs S4 + S5 in one go | B-08 (Q-01 item 3) | docs and a script only | S, 1 h | revert |
| 0.7 | **Gate:** S1-S6 all pass on the original code (with 0.3-0.6 in); the run time of each part recorded | brief 5.1 | — | S, 1 h + run time | — |

Step 0 total: about 43 hours plus the game-dependent run times (not
measured yet).

## Steps 1-11, ordered by risk (lowest first)

Every step: one commit; the full suite after it; rollback = `git revert`
of that commit (the steps are independent unless "after" is named).

| # | What changes | Why (B) | How behaviour is shown unchanged | Effort | Risk | Rollback |
|---|---|---|---|---|---|---|
| 1 | SPDX header in the 19 tracked `.py`/`.sh` files without one (scripts/ 6, tools/ 9, docker/ 3, tests/ 1; counted in stage 4); `SOURCE` in `scripts/check-repo-policy.py` also covers `.py` and `.sh` | B-26 | comment lines only; S6 identical binaries; each script's `--help` or dry output unchanged; S1 passes | S, 1.5 h | none | revert |
| 2 | `scripts/check-vendored.py` checks every SHA-256 recorded in `THIRD_PARTY.md` / `VENDORED.md`; run in the lint job and `pre-push.sh`. Mismatches found → B-43+, not fixed (the check then lists them as known, so CI stays green) | B-25 | no build input changes; S6 identical | S, 3 h | none | revert |
| 3 | Policy script and `.gitignore`: SCI0 patch files `TYPE.NNN`, `*.sav`, `sierra-hd.ini` anywhere, with a self-test of names to reject and to accept | B-15 (G-18) | S6 identical; self-test; policy passes on the tree | S, 2 h | a pattern too wide (the accept list catches it) | revert |
| 4 | "Licensing" section in `hints/README.md`, marked ASSUMED (Q-03), and a `docs/ASSUMPTIONS.md` entry | B-12 | docs only; `scripts/check-hints.py` passes; S6 identical | S, 2 h | none | revert |
| 5 | macOS removed: `ci.yml` (option, plan output, job, comments), `release.yml` (input, job, header), `native-toolchain/action.yml` (macOS branch), `CMakePresets.json` display names, `cmake/SciCompilerFlags.cmake:48` (`NOT APPLE`) and `:59` (message), `scripts/ci-minutes.py`, `scripts/package-release.sh:7`, `README.md` macOS section, the two HiDPI comments reworded, `docs/ARCHITECTURE.md:320`, a `docs/ASSUMPTIONS.md` entry. SDL3's Apple back-ends stay (B-01 default) | B-01, B-16 | S6: compile and link commands identical on Linux (the `-fuse-ld=lld` condition is unchanged off Apple); the Windows presets are unaffected (`SCI_CLANG_CL` branch untouched); workflow files parse; a manual `quick` CI run if CI minutes are allowed; S2-S5 | M, 6 h | a YAML slip silently drops a job: compare the job list of a dispatched run with the one before | revert |
| 6 | Delete `component_name()` and its two `component.cpp` files with declarations | B-24 (G-43) | S6: only those translation units vanish; S2-S5 | S, 0.5 h | none (the linker would name a caller) | revert |
| 7 | Remove the unused `DEPENDS` edges: sci-pic, sci-view, sci-text → sci-resource; sci-render → sci-view | B-21 (G-31) | S6: only `-I` and link-order differences, listed in the commit; S2-S5 | S, 1 h | a missing transitive include fails the build (visible, not behavioural) | revert |
| 8 | `sci_test_private_includes(<test> <library>)` in `cmake/SciTesting.cmake` replaces the five ad-hoc private include lines of the tests (and is used by step 0.5's targets if they need it) | B-23 | S6: test compile commands identical; same test count and results; `sierra-hd` identical | S, 2 h | none | revert |
| 9 | Move `src/app/platform` → `src/sci-platform` (`git mv`); `CMakeLists.txt` adds `add_subdirectory(src/sci-platform)` before `src/app`; `src/app/CMakeLists.txt` drops `add_subdirectory(platform)`; the two doc lines naming the old path. Target, include paths (`sci/platform/…`) and executable path unchanged | B-02 (Q-07), G-29 | S6: commands identical after replacing the path; S2-S5; `git log --follow` | S, 3 h | assertion messages name the new path (only on failure); open branches touching these files conflict | revert |
| 10 | Move `reports/` → `docs/reports/` (`git mv`) and every reference: `ci.yml:19`, `:28`, `:100`; `.gitignore` `/reports/images/`, `/reports/dumps/`; `scripts/check-repo-policy.py:56`; `tools/room-viewer/main.cpp:25` (comment, same line count); `tools/room-viewer/README.md:26`, `tools/hd-frames/README.md:54`, `tools/hd-report/README.md:30`; `README.md:17`, `:103`, `:111`; the `reports/` paths in `docs/` (34 lines outside `review/`, from `git grep`). `review/` is not rewritten (it cites `a0422d6`); `review/STATUS.md` gets the path mapping | B-02 (Q-07) | S6 identical binaries (the comment edit keeps line numbers); acceptance grep of B-02; S1-S5 | M, 4 h | a missed path in CI `paths-ignore` makes doc-only pushes start CI (cost, not behaviour) | revert |
| 11 | `local/` with `README.md` and `game/.gitkeep`, `rom/.gitkeep`, `saves/.gitkeep`; `.gitignore` rule; the policy script rejects anything else under `local/`; examples in docs and tool READMEs use `local/game` and `local/rom` (a new `docs/ASSUMPTIONS.md` entry, the old one kept). The program does **not** look there yet: that is B-03 in iteration 2 | B-02 (Q-07), B-15 | S6 identical; `git check-ignore -v` on sample paths; policy self-test with `local/game/RESOURCE.000` | S, 1 h | none | revert |
| 12 | **Close:** S1-S6; `cross-aarch64` with S4; Windows presets; ScummVM comparisons (end); 10 min fuzzing per target; `review/REFACTOR_REPORT.md` with tests, warnings, sanitizers and timings before vs after, bugs found (B-43+), remaining work | brief 5.4 | before/after tables | S, 4 h + run times | — | — |

Steps 1-12 total: about 30 hours plus run times. Stage 5 total: about 73
hours plus the game-dependent run times.

## Why the layout move is in stage 5, not iteration 2

- **It is behaviour-neutral.** Steps 9-11 move files and edit paths; the
  CMake target names, include paths and the executable path stay; S6 shows
  the build commands are the same up to the renamed path, and S4/S5 show
  the outputs are the same.
- **It blocks iteration 2.** B-03 (the data folder) uses `local/` for
  development runs, and every later item edits files whose paths the move
  changes. Moving first means iteration 2's commits, reviews and
  `git blame` all start on the final tree, instead of every open item
  rebasing over a big move later.
- **It reduces risk.** A move done in the same commit as behaviour changes
  hides them in a large diff; done alone, under the full suite, it is
  provably empty of behaviour.
- **Only the program's use of `local/` is a behaviour change**, so that
  part (B-03) stays in iteration 2.

## Not in the plan (iteration 2 or later)

- Any behaviour change: B-03 (data folder), B-04 (ini in place), B-09
  (frame time), B-10 (timing mode), B-28 (log categories), B-29 (range of
  N), B-34, B-35, B-36, B-37.
- Any restructuring of sci-vm, sci-render, sci-vector, sci-audio or app
  code, including B-32 (testable app layer) and the private-test-access
  clean-up beyond the helper (B-23 rest). "Optimising" waits for measured
  misses (Q-04).
- The VM's CI net, B-06 and B-07 (decided as iteration 2 in Q-01), and the
  CI coverage job of B-08.
- Documentation refreshes beyond the paths stages 5 moves: B-13
  (`ARCHITECTURE.md`), B-14 (other docs and READMEs).
- Lint additions beyond steps 1-3: B-22, B-27, B-38.
- Investigations: B-33 (`-fwrapv`), B-39 (trigonometry) — their evidence
  comes for free from step 0.2's UBSan run and step 0.4's traces, but the
  decisions are iteration 2's.
- Fixes of any bug found in stage 5 (B-43+).
- Pruning SDL3's Apple back-ends (B-01 default: keep).
- Publishing a release (the customer's step, B-16).

## What the customer approves

1. This plan as written (steps 0-12, about 73 hours plus run times).
2. The full suite per commit, including S4 and S5 on the game copy in this
   container (allowed by answer 3 after stage 1a), or per step if one run
   proves much over an hour.
3. CI minutes for one draft pull request run of the Windows presets and
   one `quick` run after step 5 (optional; otherwise local only, Windows at
   the end).
4. The defaults of `review/BACKLOG_V2.md` marked "default — accept or
   strike" that the plan uses: B-21, B-24 (steps 6-7), B-23 stage 5 part
   (step 8), and keeping SDL3's Apple back-ends (B-01).
