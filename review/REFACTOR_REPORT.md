# Stage 5: refactoring report

Branch `refactor` from `a0422d6` (milestone 9, `origin/main`), following
`review/REFACTOR_PLAN.md` (approved 2026-11-06). This file is written as
stage 5 goes; the final before/after report (brief 5.4) is completed at
step 12. Nothing is pushed.

## Step 0 progress

| Sub-step | State | Notes |
|---|---|---|
| 0.1 branch, `review/`, two clean builds, reproducibility | in progress, blocked | `review/` brought in (commit e20328b); `git diff a0422d6 -- . ':!review'` empty; the two clean builds are blocked: docker daemon not running (below) |
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

## Baselines (before)

All measured on the original code (`a0422d6`), unless noted.

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
