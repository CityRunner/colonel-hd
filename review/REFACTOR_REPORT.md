# Stage 5: refactoring report

Branch `refactor` from `a0422d6` (milestone 9, `origin/main`), following
`review/REFACTOR_PLAN.md` (approved 2026-11-06). This file is written as
stage 5 goes; the final before/after report (brief 5.4) is completed at
step 12. Nothing is pushed.

## Step 0 progress

| Sub-step | State | Notes |
|---|---|---|
| 0.1 branch, `review/`, two clean builds, reproducibility | **done** | `review/` brought in (e20328b); `git diff a0422d6 -- . ':!review'` empty; the two clean release builds are byte-identical (below) |
| 0.2 baselines | in progress | CI minutes measured (below) |
| 0.3 golden outputs of all resources (Q-02) | not started | |
| 0.4 VM output hashes (Q-01) | draft only | `scripts/check-vm-hashes.sh` drafted, untracked and never run (needs the container); the program-run end state (B-19 criterion 4) is not in it yet |
| 0.5 four fuzz targets | not started | |
| 0.6 local VM gate written down | not started | |
| 0.7 gate: S1-S6 on the original code | not started | |

## Environment incidents

- 2026-11-06, from 07:0x to at least 07:25 UTC: the shared docker daemon
  is not running. `docker ps`: "Cannot connect to the Docker daemon"; no
  `dockerd` or `containerd` process; `/var/run/docker.pid` names pid 363,
  which does not exist. The machine was rebooted at about 06:48 UTC
  (`uptime`: up 34 min at 07:22) and the daemon was not started after the
  reboot. This session does not start it (rule); it waited with
  until-loops for about 17 minutes in total. Everything that needs the
  container (all builds, tests, sanitizers, timings, game checks) is
  blocked until someone starts the daemon. Done meanwhile: branch,
  `review/`, CI minutes, the customer's answers recorded, a draft of
  `scripts/check-vm-hashes.sh`.
- About 07:25 UTC: the main session started the daemon again (image
  `sierra-hd-toolchain:fa237f3eeea06223` present); step 0.1 resumed.
- Correction: my first hand-off report (07:25) said the MT-32 ROM folders
  were empty. That was wrong: my listing was cut off by `head`. The ROMs
  are present: `/home/user/roms/roland` holds
  `CM32L_CONTROL.1989-12-05.v1.02.ROM`, `CM32L_PCM.ROM`,
  `MT32_CONTROL.1987-10-07.v1.07.ROM`,
  `MT32_CONTROL.1988-09-30.vX.XX-patched.ROM` and `MT32_PCM.ROM`;
  `/home/user/roms/mt32-scummvm` has the symlinks `MT32_CONTROL.ROM` and
  `MT32_PCM.ROM` into `../roland`. Mount `/home/user/roms:/roms`
  (read-only) so the symlinks resolve.

## Step 0.1: reproducibility of the build

Two clean release builds of the original code (`a0422d6`; the tree of
`refactor` differs only in `review/`), one after the other in the
toolchain container (`sierra-hd-toolchain:fa237f3eeea06223`, 4 cores):

    docker/run.sh bash -c 'cmake --preset release -B build/baseline-release && cmake --build build/baseline-release'
    docker/run.sh bash -c 'cmake --preset release -B build/baseline-release-2 && cmake --build build/baseline-release-2'

| | `baseline-release` | `baseline-release-2` |
|---|---|---|
| configure | 39.6 s | 16.5 s |
| build (637 ninja steps) | 84.8 s | 81.2 s |
| warnings | 9 (all in `third_party/SDL3`, below) | 9 (same) |

**Result: the build is reproducible.** All 569 build outputs compared
(36 executables, 16 static libraries, 517 object files; everything
executable or `*.a`/`*.o` outside `CMakeFiles/`) have identical SHA-256 in
both trees. `compile_commands.json` and `ninja -t commands` (637 commands)
are identical after replacing the build folder name. So S6 can use binary
identity: `sierra-hd`, every tool and every test binary.

Key hashes of the original code (all 36 executables:
`review/stage5/baseline-release-binaries.sha256`):

| Binary | SHA-256 |
|---|---|
| `src/app/sierra-hd` | `54f59d747ebccf5435a76c40c1a0ed5148f78d953d97bc98ba21e977d1817e2a` |
| `tools/vm-trace/sci-vm-trace` | `7a57f804f7c8d2abbd0c8230fbd4dcffebc66a0ce653196bfcb10e40e84585ab` |
| `tools/golden-hashes/sci-golden-hashes` | `963d83abb03b2d4fb6ed41ea134542bd0573f422aef049f76821ff779af98d24` |

`build/baseline-release` is kept as the reference build of the original
code for the whole of stage 5; `build/baseline-release-2` is kept until
the goldens of 0.3/0.4 have been generated from both. Logs and full
manifests: `build/stage5/` (git-ignored).

## Baselines (before)

All measured on the original code (`a0422d6`), unless noted.

### Build warnings (B-20 criterion 1)

Release preset with the project's flags (`-Wall -Wextra -Wpedantic
-Wconversion ...` and `-Werror` on project targets): **0 warnings in
project code**; 9 in vendored SDL3 (`-Wunused-but-set-global`:
`SDL_events.c:119`, `SDL_gamepad.c:126`, five at
`SDL_render_vulkan.c:158`, `SDL_evdev_kbd.c:189`, `:190`). The plan asks
for `SCI_WARNINGS_AS_ERRORS=OFF`; with it ON the build succeeds, and
`-Werror` changes only a warning's severity, not which warnings appear,
so the OFF count is the same (0 in project code). The other presets:
see below as they are measured.

### CI minutes of the last gate (B-20 criterion 4)

`scripts/ci-minutes.py --branch milestone-9` (2026-11-06): 3 runs on
`milestone-9`, of which one ran the full gate (run 41010700650,
2026-10-30, ready-for-review PR; the other two ran only `plan`).

| Platform | Jobs | Minutes (billed, rounded up per job) | Included minutes consumed | List price |
|---|---|---|---|---|
| linux-x64 | 8 | 62 | 62 | $0.37 |
| linux-arm64 | 3 | 22 | 22 | $0.11 |
| windows-x64 | 1 | 12 | 24 | $0.12 |
| windows-arm64 | 1 | 4 | 8 | $0.04 |
| **total** | | **100** | **116** | **$0.64** |

Wall time of the gate run: 20 min 25 s (05:51:06 → 06:11:31 UTC), set by
the `linux-x64 fuzz` job (20 min 20 s); the longest build-and-test jobs
were `windows-x64` 11 min 38 s, `style and static analysis` 10 min 55 s,
`linux-x64 tsan` 9 min 14 s.
