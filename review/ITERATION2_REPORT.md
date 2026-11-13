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
