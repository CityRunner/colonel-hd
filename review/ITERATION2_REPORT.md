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
