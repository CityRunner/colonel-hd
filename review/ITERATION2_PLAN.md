# Iteration 2: order of work

Started 2026-11-13 on the customer's go ("Yes, run Windows CI and then
move to the next step"), on branch `iteration-2` from the end of stage 5
(`refactor`, `2ad6c60`). The items, their acceptance criteria and their
estimates are in `review/BACKLOG_V2.md`; this file only orders them into
packages. Each package is done in small commits, each item checked with
the local gate (S1-S5 as in stage 5; behaviour changes only where the
item asks for one, and then with new goldens or tests that show it), and
each package ends with a **STOP** and a short report for the customer.

| Package | Items | Why first / why here | Estimate |
|---|---|---|---|
| **P1 Robustness bugs from stage 5** | B-45 script loader, B-46 music player hang, B-44 `fuzz-save` in the list, B-49 flaky audio test, B-47 SDL revision, B-48 SDL3 hashes | Real defects; B-45 and B-46 also stop their fuzz targets after seconds, so fuzzing covers them again once fixed | about 12 h |
| **P2 Small clean-ups** | B-31 hints for pictures 55 and 992, B-35, B-36, B-27, B-33, B-22, B-38, B-39, B-17 (scan, if a copy turns up) | Low risk, each one commit; clears the ground for P3-P6 | about 17 h |
| **P3 The program's files and options** | B-04 ini updated in place, B-03 one run-time data folder (uses `local/`), B-34 test hooks, B-28 log categories, B-29 range of N, B-37 thin fallbacks in the report | Visible to the user; B-03 changes where files go | about 27 h |
| **P4 Docs** | B-13 ARCHITECTURE, B-14 other docs, B-16 release workflow rest | After P3, so the docs describe the final layout | about 19 h |
| **P5 The VM in CI** | B-06 test assembler and bytecode tests, B-07 a small home-made game, B-08 sci-vm coverage ≥ 70 % in CI | The biggest item and the biggest risk (High); until it is done, every sci-vm change needs the local gate | about 76 h |
| **P6 Testable application** | B-32 options and `GameHost`, B-23 the remaining private includes | Builds on P3 and P5 | about 26 h |
| **P7 Performance** | B-10 scripted timing mode (and the customer's laptop run), B-09 no frame over 16.7 ms at N = 10, B-11 memory check | Last: measured against the final code | about 25 h |

Not planned: B-30 (default: nothing), B-40, B-41, B-42 (struck).

CI: one `quick` run per package on GitHub, and Windows only where a
package changes CMake, platform code or the workflows (as at the end of
stage 5); everything else runs locally.
