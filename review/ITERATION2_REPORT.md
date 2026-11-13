# Iteration 2: report

Order of work: `review/ITERATION2_PLAN.md`. Branch `iteration-2` (from the
end of stage 5; stage 5 is merged into `main` as `b531169`).

## Packages P1 and P2 (2026-11-13), at STOP

### What was done

| Item | Commit | What changed |
|---|---|---|
| B-45 | `bdb1c8f` | An object with fewer than the four base properties is a load fault, not a read past its properties. All 344 saved crash inputs load without an abort; reproducer in `tests/fuzz/corpus/script`. |
| B-46 | `bdb1c8f` | A song track that runs past its data ends there, as with an end-of-track marker (before: endless zero-delta meta events, one timer call never returned). All 17 saved timeout inputs render in under 0.3 s; reproducer in `tests/fuzz/corpus/sound`. |
| B-44 | `ef2836e` | `fuzz-save` is in the fuzzed list; `scripts/check-fuzz-list.py` (lint job, pre-push) keeps every target there. |
| B-47 | `22d798f` | SDL's revision string is the vendored release (`SDL-3.4.18-829a65d769d9`), not this repository's `git describe`. |
| B-48 | `9b46cac` | `third_party/SDL3/SHA256SUMS` (1,503 files) read by `check-vendored.py`; compared with a fresh clone of `release-3.4.18`: identical apart from line endings in 7 files; 7 shader sources not committed (SDL's own `.gitignore`), recorded in `VENDORED.md`. |
| B-49 | `41afc1d` | The audio test checks what `recover()` queued, not the queue after it. |
| B-31 | `3607a24` | `hints/README.md` names pictures 55 and 992 and why they have no file. |
| B-33 | `3607a24` | Decision recorded (`docs/ASSUMPTIONS.md`): `-fwrapv` stays in Release; stage 5's UBSan runs found no signed overflow. |
| B-39 | `3607a24`, `59efc9e` | Decision recorded: the game never passes an angle to the trigonometry (only GetDistance is called, never with its angle); comment updated. |
| B-35 | `56dee1e` | The unbuilt `SCI_WITHOUT_STROKE_INTEGRITY` option removed. |
| B-36 | `6417c50` | Relation hints removed from the format, parser, schema, README and fuzz seed; a `relations` member is rejected (new test). |
| B-22 | `d64fadf` | `scripts/check-deps.py`: declared library dependencies must match the includes (lint job, pre-push). |
| B-38 | `bd85846` | `scripts/check-analyser-version.py`: the HD picture goldens change only with an `analyser_version` bump (lint job with full history, pre-push). |
| B-27 | `89107cb`, `16a33d4` | A reason on every `NOLINT` (21 bare ones); the policy check refuses a bare one. The first version's long trailing reasons were wrapped by clang-format, which moved the markers; they are now `NOLINTNEXTLINE(check): reason` lines. |
| B-17 | — | Conditional: no other release of the game is available to scan; stays open. |

`tests/fuzz/known-failures.txt` is empty again.

### Checks

| Check | Result |
|---|---|
| `fuzz-script`, 10 min (was stopped by B-45 after about 22 s) | pass: 34,675 inputs, coverage 1381 (was 1179) |
| `fuzz-sound`, 10 min (was stopped by B-46 after about 25 s) | pass |
| `fuzz-save`, 10 min, fuzzed for the first time | pass: 1,599 inputs (large inputs), coverage 1721 |
| B-47: two clean release builds of `16a33d4`, `git describe` `v0.9.0-rc.1-82-g16a33d4` and (local tag) `b47-probe-0-g16a33d4` | the same `colonel-hd` (`cbbbf9c8…`) |
| clang-tidy over the tree (after the B-27 fix-up) | clean |
| Pre-push pieces on `16a33d4`: style, release, asan, tsan, fuzz (30 s per target, no known failures), ARM64 | all pass |
| Local gate on `16a33d4`: resources, pictures, sounds, 14,731 screens, VM hashes | **all match the committed goldens** |
| ScummVM comparison (P1 code): 39 scenarios; the playthrough | 39 ok, 0 FAIL; matches to tick 338000 |
| S6 (binary identity) | differs, as expected: P1 and P2 change code |

One run of the playthrough comparison failed because the session's disk
allowance was full (outputs cut off mid-write); after deleting finished
measurement builds it was run again and matched.

### What is next

P3: the program's files and options (B-04 ini updated in place, B-03 one
run-time data folder, B-34, B-28, B-29, B-37). These change what the user
sees; the package report will list every visible change.

## Package P3 (2026-11-13), at STOP

### What was done

| Item | Commits | What changed (visible to the user) |
|---|---|---|
| B-03 | `5ad0dbb`, `932fa89`, `4e9dea1` | One data folder for `colonel-hd.ini`, `game/`, `rom/`, `saves/`, `cache/`, `files/`: `--data <folder>`, else the program's folder if it holds the ini or `game/` and is writable (portable), else `%APPDATA%\colonel-hd` / `~/.local/share/colonel-hd`. An empty `game_folder` uses `game/`, an empty `mt32_rom` uses `rom/` (not written to the ini). Hints stay next to the program. Release archives hold empty `game/`, `rom/`, `saves/` with a `README.txt`. The data folder is logged at start-up. |
| B-04 | `44524a8`, `7f72a2a` | The ini is written only when the mode, the viewer's scale or a game folder chosen in the dialog changed, and only those lines; comments, unknown keys, other sections and CR LF stay. A folder from the command line is never written. A failed write is a warning naming the file. |
| B-28 | `c3cc4d2` | `--log vm,resources` and the ini key `log` show the Vm and Resources messages; off by default. |
| B-37 | `953082e` | `sci-hd-report` lists every fallback face, thin ones included, with a count per picture. |
| B-29 | `c3dfb3e` | N stays 1 to 16; the chord comment gives the error at N = 16 (0.063 screen pixels); the targets of spec 10.3 apply up to N = 10 (ASSUMPTIONS). |
| B-34 | `1aa721e` | The testing options are kept and described in the user README. |
| — | `626da91` | clang-tidy findings in the new code fixed (functions split under the complexity limit); found by the first closing run, which failed at its style step. |

### Checks

| Check | Result |
|---|---|
| Unit tests: data folder order (option, portable with ini or `game/`, per user, read-only program folder run as an unprivileged user, non-ASCII folder name), ini update in place (LF, CR LF, missing key, no change, new file), failed write warning, log category lists and the ini key | pass |
| B-03 end to end: release archive unpacked, the game copied into `game/`, the program started with no game folder and no `--data`, the whole replayed playthrough | portable folder chosen, status 0, 0 errors and warnings, end state the same as `sci-vm-trace`'s; the ini written with the commented defaults and an empty `game_folder` |
| `scripts/test-package-release.sh` | archive layout ok (105 entries), no game data |
| B-37 on the game: `sci-hd-report` over all 88 pictures | the listed fallbacks match the table's counts for every picture: 14,940 thin and 1 mixed (before: only the 1 mixed face was listed) |
| Pre-push on `626da91`: style and clang-tidy over the tree, release, asan, tsan, fuzz, ARM64 | all pass |
| Local gate on `626da91`: resources, pictures, sounds, 14,731 screens, VM hashes (the program's playthrough now runs with `--data`) | **all match the committed goldens** |

### Not checked here

- The per-user folder on a real Windows account and a non-ASCII Windows
  user name (the code reads `APPDATA` wide; the customer's laptop run,
  B-10).
- B-28's acceptance names a script workaround warning from B-07's
  mini-game; that game comes in P5. Here: the list parser, the ini key,
  the option and its rejection of unknown names are tested.

### What is next

P4: the docs (B-13 ARCHITECTURE, B-14 other docs, B-16 the release
workflow's rest).

## Package P4 (2026-11-13), at STOP

### What was done

| Item | Commits | What changed |
|---|---|---|
| B-14 | `575b096` | Every line of the item's list fixed: `src/sci-vm/README.md` (music player and saves), SDL3's users in `cmake/SciThirdParty.cmake`, the `docker/run.sh` example, `trigonometry.hpp` and its generator (angles outside -360..360; the generator still reproduces both files), `docs/formats/replay-and-trace.md` (the program's `--replay` keeps host input, V-07), `config.hpp` and `analysis_cache.hpp` (the data folder); `hints.hpp`/`hints/README.md`, `config.cpp`'s comment and `fitting.cpp` were done with B-36, B-04 and B-29. New READMEs: `src/sci-platform`, `tools/common`, `tools/graphics-dump`, `tools/resource-dump`, `tools/playthrough`. `README.md`: the data folder, the configuration keys in the user guide, and no claim of published binaries (none is published: GitHub lists 0 releases). `docs/ASSUMPTIONS.md`: the repository is public (GitHub reports it so), a new entry. |
| B-13 | `24e52ed` | `docs/ARCHITECTURE.md`: state milestone 9 and iteration 2 P1-P3 with the date; every library with its dependencies as `check-deps.py` checks them and its responsibilities (music player and saves in sci-vm, data folder and more in sci-platform, audio output in app); the full tools list; the data flow as built (no longer "planned"); the dependency rules with `check-deps.py`; logging with `Resources`, `Vm` and `--log`; the executor's floating-point check as built; the removed placeholders and the old fallback count gone. MAP 8.4's five errors are gone. The new statements were checked against the code. |
| B-16 | `03e1404` | The release workflow's linux and windows jobs ran only for a pushed tag, so a manual run built nothing; now a manual run builds, tests and uploads the four archives, and only `publish` keeps the tag condition. The Linux job checks the archive layout first (`test-package-release.sh`, P3). |

### Checks

| Check | Result |
|---|---|
| Manual release run on `iteration-2` at `03e1404`: [run 43405956452](https://github.com/CityRunner/colonel-hd/actions/runs/43405956452) | **success**: linux-x64, linux-arm64, windows-x64, windows-arm64 built, tested and packaged (archive artifacts 2.9, 2.9, 2.0 and 1.9 MB); `publish` skipped |
| Pre-push on `03e1404`: style and clang-tidy, release, asan, tsan, fuzz, ARM64 | all pass |
| Local gate with the game | not run: P4 changes only docs, comments and a workflow (decision 1a); the last gate, on `626da91`, passed |

### Deviations and open points

- B-16 criterion 2 names a run on `main`; the run was made on
  `iteration-2`, which holds the workflow change and the P1-P3 code
  (`main` has neither yet).
- The Windows archives' contents could not be listed here: downloading
  workflow artifacts is blocked by this session's network policy. They are
  made by the same `package-release.sh` as the Linux ones, whose layout is
  tested.
- B-16 criterion 4: `docs/release-notes/<tag>.md` must exist before the
  next tag is pushed; it waits for the customer to name the tag.
- B-16 criterion 6: pushing tags is the customer's (this session cannot
  push tags); the local tags `v0.1.0` and `v0.9.0-rc.1` are the customer's
  to push or drop.

### What is next

P5: the VM in CI (B-06 a test assembler and hand-built bytecode tests,
B-07 a small home-made game, B-08 sci-vm line coverage of at least 70 % in
CI). It is the largest package (about 76 hours) and will be split into
small steps, each with a STOP-free commit and the local gate at the end.

## Package P5 (2026-11-13), at STOP

### What was done

| Item | Commits | What changed |
|---|---|---|
| B-06 | `04badd1` to `2503817` | A test-side SCI0 script assembler (`tests/unit/vm/script_assembler.hpp`: objects, classes, methods, code with labels, strings, exports, locals, and the resources a game needs) and `bytecode_test.cpp`: every opcode the interpreter accepts runs in a test (a list of tested opcodes; the four untested ones must fault "illegal opcode"), all 64 variable opcodes, sends, super, class calls, pointer arithmetic, a fault. machine.cpp's line coverage in CI went from 2.5 % to 77.3 %. |
| B-07 | `b5916dd` to `80c3a67` | A home-made game in `home_game_test.cpp`, built at test time (no binary blob): strings, lists, maths, scripts and the GC; a picture, windows, text, ports, a view, cels, an actor in a cast, Graph; dialog controls of every type, menus, a mover (InitBresen, DoBresen, SetJump); the parser on a small vocabulary (shared with the parser tests, `tests/unit/text/sample_vocabulary.hpp`), Said, synonyms; a sound initialised, played, paused, faded, stopped; the cursor; key and mouse events; the device queries; menus walked by keyboard and mouse; a save read into a fresh machine. Goldens of every run's trace and last screen and of the saved state are in `home_game_goldens.hpp`, generated by `scripts/update-home-game-goldens.sh`, which makes them twice and fails if the runs differ. |
| B-08 | `3afa581`, `f1b3de5` | `scripts/coverage.sh --min-vm 70` as a CI job of the pull request gate (Linux x64), also the manual scope `coverage`; its cost recorded in `docs/ASSUMPTIONS.md`; `tests/README.md`: the local game gate is required for releases only now, recommended before merging VM changes. |

### Checks

| Check | Result |
|---|---|
| sci-vm line coverage in CI ([run 43427816386](https://github.com/CityRunner/colonel-hd/actions/runs/43427816386), manual `coverage`) | **73.2 %** of 14,559 lines (38.4 % before P5); machine.cpp 77.3 %; the job took 3 min 44 s without a compiler cache |
| B-07 criterion 2, the kernel groups at 0 % before (MAP 6.4) | each group has calls; line coverage of their files now: gfx_animate 62 %, gfx_menu 68 %, gfx_controls 55 % (0 % before), k_strings 49 %, k_lists 74 %, k_movement 69 % (0 % before), k_math 78 %, gc 88 %; also k_sound 90 % and sound.cpp 70 % (14 % before), k_parse 53 % (11 % before), gfx_cursor 52 % (18 % before) |
| B-07 criterion 3, save and restore | the fresh machine's persistent state hash and every component but the execution equal the saved one |
| B-07 criterion 4, goldens twice | `update-home-game-goldens.sh`: 11 goldens, both runs equal (run three times, after each change) |
| B-07 criterion 1, time | the B-06 and B-07 tests take 0.3 s under ASan and under 1 s each in Debug |
| Pre-push on `3afa581`: style and clang-tidy, release, asan, tsan, fuzz, ARM64 under qemu | all pass |
| Local gate on `3afa581`: resources, pictures, sounds, screens, VM hashes | **all match the committed goldens** |
| Windows CI on `3afa581` ([run 43433464586](https://github.com/CityRunner/colonel-hd/actions/runs/43433464586)) | **failed**: `std::println` to a stream does not compile with MSVC's library; fixed in `80c3a67` |
| Windows CI on `80c3a67` ([run 43436461968](https://github.com/CityRunner/colonel-hd/actions/runs/43436461968)) | **success**: windows-x64 (release, ASan) and windows-arm64 (release) built and passed every test |

### Deviations and open points

- B-28's acceptance names a script workaround warning from the home-made
  game. Its scripts trigger no workaround (they are not the game's
  scripts), so that one check is still open; the log categories are
  tested by P3's tests.
- The whole sci-vm test binary takes 54 s in the Debug preset, almost all
  of it in two older save tests (28 s and 14.5 s), not in P5's tests. Not
  changed here; if it grows, those two tests are the place to look.
- The coverage job's measured cost is from one run without a compiler
  cache; with the cache, later runs should be shorter (not measured).
- B-06's criterion "all CI presets": Linux release, ASan, TSan and ARM64
  ran locally (pre-push) and on CI's `coverage` job; Windows is the run
  above.

### What is next

P6: a testable application (B-32 options and `GameHost`, B-23 the
remaining private includes).

## Package P6 (2026-11-13), at STOP

### What was done

| Item | Commits | What changed |
|---|---|---|
| B-32 | `8b5b06f`, `7aafe77`, `603d661` | The program's pure parts move from `src/app` to sci-platform, each with CTest tests: the command line (`command_line.hpp`: options, the R-110 rules, the help text, the exit codes 0/0/2, and the defaults the data folder gives), the window scale (`window_scale.hpp`, R-112: auto, a configured N reduced to what fits, the opening scale; the game and the viewer share it), and the game's file store (`file_store.hpp`: written files in `files/`, read before the game folder's, plain names only). `main.cpp` went from 1,322 to 1,207 lines. Nothing visible changes. |
| B-23 | `34a26bd` | sci-vector's tests use its public interface only: curve fitting is a public header (`sci/vector/fitting.hpp`, namespace `sci::vector`), and `sci-vector-tests` no longer include `src/sci-vector/src`. The helper `sci_test_private_includes` documents the two remaining exceptions: sci-vm (its internals are what its tests check, B-06/B-07) and app (a program, no public interface). |

### Checks

| Check | Result |
|---|---|
| New tests: every R-110 rule (no arguments, `--version`/`--help` alone and with more, all game options, the viewer with data and log, the viewer with each game option, bad seeds incl. 2^32, unknown log category, a missing value, an unknown option, two folders), the data-folder defaults, the scale rules, the file store | pass (sci-platform-tests 52, was 40) |
| The program's exit codes by hand: `--help` 0, `--version` prints `colonel-hd 1.0.0`, `--viewer --fast` 2 | as before |
| All CTest tests in Debug | 31 of 31 pass |
| Pre-push on `603d661` | all pass: style and clang-tidy over the tree, release, asan, tsan, fuzz, ARM64 under qemu |
| Local gate on `603d661` (B-18, B-19, the program's playthrough end state) | **all match the committed goldens**: resources, pictures, sounds, 14,731 screens, VM hashes (the program's playthrough end state included) |
| Windows CI on `603d661` ([run 43440744960](https://github.com/CityRunner/colonel-hd/actions/runs/43440744960)) | **success**: windows-x64 (release, ASan) and windows-arm64 (release) |

### Deviations and open points

- `GameHost`, the presenter and the event loop stay in `main.cpp`: they
  are SDL and timing code, and B-32's default moves only pure functions
  (its risk note). They remain untested by CTest; the program's
  playthrough in the local gate covers them.
- `write_end_state` and `read_replay` (test hooks of the program, G-151)
  stay in `main.cpp`; P7's timing mode (B-10) will touch them.

### What is next

P7: performance (B-10 a scripted timing mode and the customer's laptop
run, B-09 no frame over 16.7 ms at N = 10, B-11 the memory check).

## Package P7 (2026-11-13), at STOP

### What was done

| Item | Commits | What changed |
|---|---|---|
| B-10 | `63daa5f` to `4a9ffda` | `colonel-hd --replay <file> --timing <csv> [--scale N]`: the replay played as fast as it can with every frame presented at a fixed N (also when the window is smaller), and a CSV of the run: frame-time percentiles (the main thread's time per presented frame, and the composing part alone), each room entry (from the request of the new picture's HD images to the first frame composed with them, the game paced in real time meanwhile as in normal play), the first launch, peak memory, and every frame over 16.7 ms with its tick, room, composing time and upload size. In the timing mode the window's pointer is not the game's input (at N = 10 the frame is larger than the window and the pointer's round trip changed the game). The statistics and the CSV live in sci-platform (`timing.hpp`) with tests; `--timing`/`--scale` and their rules are in `command_line.hpp` with tests. `docs/timing.md` is the one-page instruction for the customer's run; `tools/timing/run-timing.cmd` (Windows) and `run-timing.sh` (Linux) run N = 1, 2, 5, 10; `tools/playthrough/program-run.sh --timing` runs it in the container and checks the end state. Normal runs are unchanged (the local gate, below). |
| B-11 | `42c2e60`, `354d682`, `3edaad9` | `scripts/check-memory.sh`: the timing mode at N = 1 and 10, failing above `tests/golden/memory.txt` + 10 %; `--update` writes the medians of 3 runs. Baseline: 165,936 KiB at N = 1, 378,868 KiB at N = 10. Listed among the checks before a release (`tests/README.md`). |
| B-09 | — (measured, nothing changed) | See below: the misses measured here are in the presentation, which the container renders in software; no optimisation before the customer's laptop run. |

### Measurements (development container, 4 cores, Xvfb software rendering, warm cache)

The playthrough in `colonel-hd --timing`; every run ended in the state of
`sci-vm-trace` (same tick, globals and state hash).

| N | Runs | Frames presented | Frame ms: median / p99 / worst | Frames over 16.7 ms | Composing worst | Room entry ms: median / worst | Peak memory |
|---|---|---|---|---|---|---|---|
| 1 | 3, median | 37,912 | 0.94 / 1.53 / 20.9 | 1 (0-1) | 3.6 ms | 17 / 34 | 165,936 KiB |
| 4 | 1 | 20,147 * | 3.48 / 5.04 / 25.0 | 2 | — | 97 / 509 * | 203,508 KiB |
| 10 | 5, median | 37,823 | 6.97 / 10.70 / 43.1 | 59 (55-65) | 19.5 ms | 233 / 327 | 378,868 KiB |

\* The N = 4 run was the first version, before room entries were paced in
real time; its numbers are not comparable with the others.

Reading N = 10 (per-frame detail from the `slow` lines of one run, 65
frames over 16.7 ms):

- 49 of the 65 upload the whole frame (6.4 million pixels): a room entry
  or the HD images arriving. Their composing takes 10 to 13 ms; the rest
  of the frame time is the upload and presentation, which Xvfb does in
  software (20 to 40 ms for a 25 MB frame).
- One frame's composing is over the budget: the game's first frame (tick
  13, 18.9 ms).
- The others are small uploads with occasional outliers (one of 176 ms in
  one run, 43 ms otherwise): noise of the shared container.
- Room entry is within 1 s at every N (worst 349 ms of 5 runs at N = 10).

So B-09's criterion 1 is not met in the container (median 59 frames over
16.7 ms at N = 10), but the composing, the part the program controls
directly, is within the budget except for the first frame. Whether the
presentation misses on real hardware (a GPU upload of 25 MB takes a few
milliseconds) is what the customer's laptop run (B-10 criterion 3) shows.

### Checks

| Check | Result |
|---|---|
| New tests: the timing statistics (nearest-rank percentiles), the CSV, peak memory read; `--timing`/`--scale` rules | pass (sci-platform-tests 56) |
| Pre-push on `3edaad9` | all pass: style and clang-tidy over the tree, release, asan, tsan, fuzz, ARM64 under qemu |
| Local gate on `3edaad9` | **all match the committed goldens**: resources, pictures, sounds, 14,731 screens, VM hashes and the program's playthrough end state (normal runs unchanged) |
| Windows CI on `3edaad9` ([run 43486277989](https://github.com/CityRunner/colonel-hd/actions/runs/43486277989)) | **success**: windows-x64 (release, ASan) and windows-arm64 (release); `peak_memory_kib` compiles on Windows (psapi) |

### Deviations and open points

- **B-10 criterion 3 (the customer's run) is open:** `docs/timing.md`
  says how. The build: a manual "Release" run on `iteration-2` (or `main`
  after the merge), artifact `colonel-hd-windows-x64`.
- **B-09 is open, waiting for that run.** Proposed: if the laptop shows
  frames over 16.7 ms at N = 10, B-09 becomes High (Q-04) and the work is
  in the presentation of full frames (for example uploading a new
  picture's frame in parts over the frames while the old one is held);
  if not, B-09 closes with the measurements. Its criterion 1 also names a
  room-entry sweep over every room; the playthrough enters 55 rooms (one
  `room` line each) and no separate sweep was made.
- B-09 criterion 4 (before/after of 5 runs): only "before", since nothing
  changed.
- The timing mode's room entries are paced in real time, so a run takes
  about 6 minutes at N = 10 here (2 minutes at N = 1).

### What is next

The customer's laptop run (`docs/timing.md`), then B-09 by its result;
then the v1.0.0 tag (the customer's).
