# Backlog

The work items of the 1.0 refactoring and iteration, by number. Code,
tests and scripts refer to them as `B-NN`.

| Item | Title | Kind | State |
|---|---|---|---|
| B-01 | Drop macOS completely | requirement | Done (1.0 refactoring) |
| B-02 | Logical repository layout | requirement | Done (1.0 refactoring) |
| B-03 | One run-time data folder | requirement | Done |
| B-04 | Ini updated in place | requirement (defect) | Done |
| B-05 | Fuzz targets for scripts, sound, patch resources, ini | requirement | Done (1.0 refactoring) |
| B-06 | VM in CI: test assembler and hand-built bytecode tests | requirement | Done |
| B-07 | VM in CI: a small home-made game | requirement | Done |
| B-08 | sci-vm line coverage in CI of at least 70 % | requirement | Done |
| B-09 | No frame over 16.7 ms at N = 10 (measured misses only) | performance | Done |
| B-10 | Timing mode and a run on a reference laptop | requirement | Done (Windows hardware not yet measured) |
| B-11 | Peak memory ≤ baseline + 10 % (regression check) | performance | Done |
| B-12 | Hint licensing section in `hints/README.md` | requirement | Done (1.0 refactoring) |
| B-13 | `docs/ARCHITECTURE.md` brought up to date | maintainability | Done |
| B-14 | Other stale docs, comments and missing READMEs | maintainability | Done |
| B-15 | Policy script and `.gitignore`: SCI0 patch files, run-time folders | defect | Done (1.0 refactoring) |
| B-16 | Release workflow: manual runs and publishing | requirement | Done |
| B-17 | `kDoAvoider` stub: scan other releases if a copy turns up | defect (latent) | Open: only if another release of the game turns up |
| B-18 | Golden outputs for all resources | maintainability (safety net) | Done (1.0 refactoring) |
| B-19 | Hashes of every VM output | maintainability (safety net) | Done (1.0 refactoring) |
| B-20 | Baselines: warnings, sanitizers, timings, memory, size, build time | maintainability (safety net) | Done (1.0 refactoring) |
| B-21 | Remove the four unused dependency edges | architecture | Done (1.0 refactoring) |
| B-22 | Check in lint that every declared edge is used | maintainability | Done |
| B-23 | Tests that include private `src/` folders | architecture | Done |
| B-24 | Remove the dead `component_name()` placeholders | tech debt | Done (1.0 refactoring) |
| B-25 | Check the vendored files' SHA-256 | maintainability | Done (1.0 refactoring) |
| B-26 | SPDX headers on scripts; policy checks `.py`/`.sh` | maintainability | Done (1.0 refactoring) |
| B-27 | A reason on every `NOLINT` | maintainability | Done |
| B-28 | Switchable log categories in the program | requirement | Done (one check open: a script-workaround message on the real game) |
| B-29 | The range of N (cap or keep 16) | requirement | Done |
| B-30 | Other sound devices | requirement | Not planned |
| B-31 | Hint files for pictures 55 and 992 | maintainability | Done |
| B-32 | Make the application layer testable (options, `GameHost`) | tech debt | Done |
| B-33 | `-fwrapv` in some build types only | tech debt | Done |
| B-34 | Test hooks in the shipped program | requirement | Done |
| B-35 | Remove the unbuilt `SCI_WITHOUT_STROKE_INTEGRITY` option | tech debt | Done |
| B-36 | Remove relation-hint parsing; fix its comments | tech debt | Done |
| B-37 | List thin fallbacks in the replacement report | requirement | Done |
| B-38 | Check the `analyser_version` rule in CI | maintainability | Done |
| B-39 | Trigonometry outside -360…360: compare with glibc | defect (unverified) | Done |
| B-40 | 5.25" release: hint warning possibly false | defect (unverified) | Dropped |
| B-41 | LGPL static linking note | maintainability | Dropped |
| B-42 | `uint32_t` sums instead of `uint64_t` | tech debt | Dropped |
| B-43 | Rename the project `sierra-hd` → `colonel-hd` | requirement | Done (1.0 refactoring) |
| B-44 | `fuzz-save` missing from the fuzz target list | defect | Done |
| B-45 | Script loader reads past an object's properties on a malformed script | defect | Done |
| B-46 | The music player can loop forever on a malformed song | defect | Done |
| B-47 | The program embeds the repository's `git describe` as SDL's revision | defect | Done |
| B-48 | SDL3 has no per-file SHA-256 record | maintainability | Done |
| B-49 | `sci-audio-output-tests` fails now and then on a loaded machine | defect (test) | Done |

## Open

- **B-17** `kDoAvoider` is a stub: the supported release never calls it
  (`sci-vm-trace --scan-kernel`). If another release of the game is
  supported, scan it; if it calls `kDoAvoider`, port it from ScummVM
  (`engines/sci/engine/kmovement.cpp`) with a bytecode test.
- **B-28** `--log vm` is tested on its parsing and the ini key; a check
  that a script-workaround message appears with the real game is open.
- **B-10** The timing mode (`docs/timing.md`) has been run on a Linux
  laptop; a run on Windows hardware is open.
