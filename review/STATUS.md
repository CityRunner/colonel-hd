# Audit status

The customer's brief is `review/TASK.md`. This file is kept current at
every stage so the work can resume in a new session.

## Current stage

**Stage 1b, recover requirements: in progress** (the customer said
"continue" after stage 1a on 2026-11-05).

| Stage | File | State |
|---|---|---|
| 1a Codebase map | `review/MAP.md` | done |
| 1b Recover requirements | `review/REQUIREMENTS_RECOVERED.md` | in progress |
| 2 Reconcile | `review/GAPS.md` | not started |
| 3 Grilling | `review/QUESTIONS.md` | not started |
| 4 Backlog and plan | `review/BACKLOG_V2.md`, `review/REFACTOR_PLAN.md` | not started |
| 5 Refactoring | `review/REFACTOR_REPORT.md` | only after the customer approves the plan |

## Decisions in force

- **Scope: `main` as merged = milestone 9** (`a0422d6`). Source:
  `docs/ASSUMPTIONS.md`, "Milestone 9 approved; milestones 10, 11 and 12
  declined (2026-11-05)": "Merge 9, decline 10, 11 and 12. Start
  refactoring and optimising." The audit "audits `main`, the code as
  merged". **Decided (customer).**
- **Milestone 10** (HD background quality, addendum A4) is declined; its
  work stays on branch `milestone-10` (and local task branches
  `m10-*`), not merged, **out of scope unless the customer says
  otherwise.** Golden hashes and frozen hint files on `main` stay those of
  milestone 9. **Decided (customer).**
- **Milestones 11 and 12** (ScummVM-compatible saves, vectorised font
  glyphs) are declined. **Decided (customer).**
- The customer's manual playthrough (part of milestone 9's exit criterion)
  was deferred and waived for the merge. **Decided (customer).**
- **Working rules (brief):** stages with a STOP after each; stages 1-4
  read-only (only `review/` is written); stable IDs R-, G-, Q-, B-;
  severity scale Critical/High/Medium/Low; never present guesses as facts.
- **Visual reviews** in the audit use the format of A4 part B
  (`docs/ASSUMPTIONS.md`, "Milestone numbers after A4"). **Decided.**
- Work happens on branch `audit` (from `main` at `a0422d6`); nothing is
  pushed without the customer's instruction.

- **Answers after stage 1a (2026-11-05)**, the customer's words:
  1. "Yes, continue": stage 1b starts. **Decided (customer).**
  2. "Delete milestone 10 branch": the local branch `milestone-10` is
     deleted. The remote branch could not be deleted from this session
     (GitHub refuses branch deletion through the session's proxy, as it
     refuses tags); the customer deletes it on GitHub. The local task
     branches and worktrees `m10-*` remain until the customer confirms
     their removal (a blanket forced removal was refused by the
     session's safety check). Milestone 10 is out of the audit's scope.
     **Decided (customer).**
  3. "Yes": later stages may run the local game-dependent checks
     (scenarios against the reference ScummVM, the playthrough, golden
     hashes) on the game copy in this container. **Decided (customer).**
  4. "If it is not implemented, then drop it." Read as: requirements of
     the original spec (v15, the copy given at the start of the project,
     plus `docs/SPEC_ADDENDA.md`) that `main` does not implement are
     dropped from scope: stage 2 classifies them "not implemented —
     dropped by the customer", not as gaps to fix, and iteration 2 does
     not take them up. The spec is not committed to the repository.
     **ASSUMED reading, to be confirmed at stage 3.**
  5. "Also: rearrange folders and files in logical way. Prepare empty
     folders for files: game, rom, saves, etc." A requirement for
     iteration 2: a logical repository layout, and empty, git-ignored
     folders for the user's own files (game files, MT-32 ROMs, saves,
     and the like), kept by a placeholder file, never with game data.
     Stages 1-4 are read-only, so it goes into the backlog and the
     refactoring plan (stage 4) and is done at stage 5 after the plan is
     approved. **Decided (customer); its timing follows the brief.**

## Open items

- Stage 1a observations to carry into stages 1b-4 (`review/MAP.md`,
  section 8): CI coverage of sci-vm (38 % lines) and of the app layer;
  the local-only game-dependent checks being the VM's real safety net;
  pixel golden outputs for pictures only; stale docs.
- The remote branch `milestone-10` (customer to delete on GitHub) and the
  local `m10-*` branches/worktrees (customer to confirm removal).
- Confirm the reading of answer 4 (stage 3).

## How to resume in a new session

1. `cd /home/user/sierra-hd && git checkout audit && git log --oneline -5`.
2. Read `review/TASK.md` (the brief), this file, then `review/MAP.md`.
3. Continue with the next stage only after the customer's "continue".
4. Scratch outputs go under `build/` (git-ignored). The coverage build of
   stage 1a was deleted to save disk; its logs and `llvm-cov` reports are
   in `build/audit-coverage/` (`report-files.txt`: CI suite;
   `report-play.txt`: playthrough). Reproduce with the commands in
   `review/MAP.md`, section 6.4. The toolchain image is
   `sierra-hd-toolchain:fa237f3eeea06223` (`docker/image-tag.sh`); docker
   is shared, do not start or stop the daemon.
