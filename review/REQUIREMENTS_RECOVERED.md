# Stage 1b: requirements recovered from the code

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-05.
Read-only: nothing outside `review/` was changed. DRAFT, in progress.

How to read this file:
- **R-xx** is a requirement the code *actually implements* (recovered from
  code and tests, not from the docs).
- **FACT** carries a `file:line` reference to the code that implements it,
  and where one exists the test that checks it.
- **HYPOTHESIS** carries a confidence (high/medium/low) and the rationale;
  it was not verified.
- **Tested**: `CI` (a CTest test run in every CI preset, `review/MAP.md`
  6.1), `local` (a game-dependent check, MAP.md 6.3), `untested` (no test
  found; MAP.md 6.4 lists the files linked into no test binary).

---

## A. Game folder identification and supported releases

| ID | Requirement | Kind | Evidence | Tested |
|---|---|---|---|---|
| R-01 | The game folder must contain `RESOURCE.MAP` (case-insensitive name match); otherwise the program refuses with "There is no RESOURCE.MAP in this folder". | FACT | `src/app/platform/src/identify.cpp:92-96`, `:72-77`; test `tests/unit/platform/platform_test.cpp:71-74` | CI |
| R-02 | The tested release (English DOS 3.5" 1.000.046) is identified by size + SHA-256 of `RESOURCE.MAP` and `RESOURCE.001`-`.004`; if the map matches but a volume differs or is missing, the folder is refused as damaged/modified. | FACT | `identify.cpp:25-51`, `:97-105` | local (needs the game; no CTest for the positive case) |
| R-03 | The patch files `SCRIPT.052` and `TEXT.052` are checked by size + SHA-256; a missing or different patch gives a warning only, not a refusal. | FACT | `identify.cpp:53-64`, `:111-121`; shown as a message box `src/app/src/main.cpp:374-377` | untested |
| R-04 | The English DOS 5.25" release is recognised by ScummVM's detection entry (MD5 of the first 5000 bytes of `RESOURCE.MAP`, size 12030) and accepted with a warning that it is untested and hints are not used. | FACT | `identify.cpp:66-70`, `:124-135` | untested |
| R-05 | Any other folder is refused (`ErrorCode::Unsupported`). | FACT | `identify.cpp:136-138`; test `platform_test.cpp:75-79` | CI |
| R-06 | Hints are used only for the picture data they were made for (PIC resource SHA-256 in the hint file); for any other data the hint file is skipped with an info log. This, not the release identification, is what keeps hints off the 5.25" release. | FACT | `src/app/platform/src/picture_hints.cpp:31-38`; `Release::Floppy525` is read nowhere outside `identify.cpp` (`grep -rn Floppy525 src tools`) | CI (hint matching: `tests/unit/vector/`) — HYPOTHESIS medium for the test location, see section D |
| R-07 | The interpreter files (SCIV.EXE, drivers) are not needed and not checked. | FACT | `identify.cpp:23-24` (only resource and patch files are listed) | n/a |
| R-08 | The game folder is read into memory as a whole (`GameFile` list) before identification. | FACT | `src/app/platform/src/game_files.cpp:12`; `main.cpp:363` | untested (`game_files.cpp` in no test binary, MAP.md 6.4); fuzzed by `fuzz-game-folder` corpus replay [target name per MAP.md 6.1] |

---

## N. The program

(in progress)
