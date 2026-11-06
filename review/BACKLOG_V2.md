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
- **default — accept or strike** — a batch-2 candidate or a PART
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
| B-12 | Hint licensing section in `hints/README.md` | requirement | High | S (2 h) | stage 5 (docs) | ASSUMED | Q-03, G-24, HISTORY_CLEANUP E |
| B-13 | `docs/ARCHITECTURE.md` brought up to date | maintainability | Medium | M (10 h) | iteration 2 | default — accept or strike | G-44, MAP 8.4 |
| B-14 | Other stale docs, comments and missing READMEs | maintainability | Low | M (6 h) | iteration 2 | default — accept or strike | G-155, G-156, G-39, G-126, MAP 8.5, 8.6, 8.13, V-03, V-05, V-07, V-11 |
| B-15 | Policy script and `.gitignore`: SCI0 patch files, run-time folders | defect | Medium | S (2 h) | stage 5 | Decided (Q-07) | G-18, V-06, R-135 |
| B-16 | Release workflow kept working without macOS; publishing is the customer's | requirement | Medium | S (3 h) | stage 5 (macOS part) / iteration 2 | Decided | G-125, R-141 |
| B-17 | `kDoAvoider` stub: scan other releases if a copy turns up | defect (latent) | Low | S (1 h) | iteration 2, conditional | default — accept or strike | R-65, R-66 |
| B-18 | Golden outputs for all resources | maintainability (safety net) | High | M (16 h) | stage 5 step 0 | Decided | Q-02, V-04, R-57, MAP 8.3 |
| B-19 | Hashes of every VM output | maintainability (safety net) | High | M (10 h) | stage 5 step 0 | Decided | Q-01, MAP 6.3 |
| B-20 | Baselines: warnings, sanitizers, timings, memory, size, build time | maintainability (safety net) | Medium | M (6 h) | stage 5 step 0 | Decided | Q-04, brief 5.1 |
| B-21 | Remove the four unused dependency edges | architecture | Low | S (1 h) | stage 5 | default — accept or strike | G-31, MAP 1.3, 8.7 |
| B-22 | Check in lint that every declared edge is used | maintainability | Low | S (3 h) | iteration 2 | default — accept or strike | G-31, MAP 1.4 |
| B-23 | Tests that include private `src/` folders | architecture | Low | S (2 h) stage 5; M (12 h) iteration 2 | stage 5 (make explicit) / iteration 2 | stage 5 part: plan; rest default — accept or strike | G-31, MAP 1.4, 8.9 |
| B-24 | Remove the dead `component_name()` placeholders | tech debt | Low | S (0.5 h) | stage 5 | default — accept or strike | G-43, MAP 8.8 |
| B-25 | Check the vendored files' SHA-256 | maintainability | Medium | S (3 h) | stage 5 | plan | MAP 5, 8.11 |
| B-26 | SPDX headers on scripts; policy checks `.py`/`.sh` | maintainability | Low | S (1.5 h) | stage 5 | plan | MAP 8.10, R-135 |
| B-27 | A reason on every `NOLINT` | maintainability | Low | S (2 h) | iteration 2 | default — accept or strike | G-36, R-136 |
| B-28 | Switchable log categories in the program | requirement | Low | S (4 h) | iteration 2 | default — accept or strike | G-41, V-10, R-121 |
| B-29 | The range of N (cap or keep 16) | requirement | Low | S (2 h) | iteration 2 | default — accept or strike | G-148, V-03, R-50 |
| B-30 | Other sound devices | requirement | Low | — (0 h with the default) | — | default — accept or strike | G-145 |
| B-31 | Hint files for pictures 55 and 992 | maintainability | Low | S (0.5 h) | iteration 2 | default — accept or strike | G-79 |
| B-32 | Make the application layer testable (options, `GameHost`) | tech debt | Medium | M (14 h) | iteration 2 | default — accept or strike | MAP 8.2, R-110, MAP 6.4 |
| B-33 | `-fwrapv` in some build types only | tech debt | Low | S (3 h) | iteration 2 | default — accept or strike | MAP 8.12 |
| B-34 | Test hooks in the shipped program | requirement | Low | S (1 h) | iteration 2 | default — accept or strike | G-151, R-115…R-117 |
| B-35 | Remove the unbuilt `SCI_WITHOUT_STROKE_INTEGRITY` option | tech debt | Low | S (1 h) | iteration 2 | default — accept or strike | G-153 |
| B-36 | Remove relation-hint parsing; fix its comments | tech debt | Low | S (2 h) | iteration 2 | default — accept or strike | G-154, G-97, V-02, R-36 |
| B-37 | List thin fallbacks in the replacement report | requirement | Low | S (2 h) | iteration 2 | default — accept or strike | G-69, R-128 |
| B-38 | Check the `analyser_version` rule in CI | maintainability | Low | S (2 h) | iteration 2 | default — accept or strike | V-12, G-156 |
| B-39 | Trigonometry outside -360…360: compare with glibc | defect (unverified) | Low | S (2 h) | iteration 2 | default — accept or strike | V-05, G-156 |
| B-40 | 5.25" release: hint warning possibly false | defect (unverified) | Low | — | — | default — strike | V-01, V-14, R-06 |
| B-41 | LGPL static linking note | maintainability | Low | — | — | default — strike | V-09 |
| B-42 | `uint32_t` sums instead of `uint64_t` | tech debt | Low | — | — | default — strike | G-73, R-46 |

B-43 onward: reserved for bugs found in stage 5.

