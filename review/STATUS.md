# Audit status

The customer's brief is `review/TASK.md`. This file is kept current at
every stage so the work can resume in a new session.

## Current stage

**Stage 1a, codebase map: done. Waiting at [STOP]** for the customer's
explicit "continue" before stage 1b (recover requirements,
`REQUIREMENTS_RECOVERED.md`).

| Stage | File | State |
|---|---|---|
| 1a Codebase map | `review/MAP.md` | done, at STOP |
| 1b Recover requirements | `review/REQUIREMENTS_RECOVERED.md` | not started |
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

## Open items

- Stage 1a observations to carry into stages 1b-4 (`review/MAP.md`,
  section 8): CI coverage of sci-vm (38 % lines) and of the app layer;
  the local-only game-dependent checks being the VM's real safety net;
  pixel golden outputs for pictures only; stale docs.
- To ask the customer (formal Q-IDs come at stage 3):
  - Is milestone 10 (branch `milestone-10`) really out of scope for the
    audit, including its findings about HD background quality?
  - May the audit run the local game-dependent checks (scenarios against
    the reference ScummVM, playthrough, golden hashes) on the game copy
    in this container (`/home/user/game/colonels-bequest-gog`) as part of
    later stages? (Stage 1a used it only for a coverage measurement.)
  - Where are the "original requirements" for stage 2: the Claude Docs
    spec "Task Spec: HD Colonel's Bequest (SCI0)", version 15
    (`docs/ASSUMPTIONS.md:22`) plus `docs/SPEC_ADDENDA.md`? The spec
    document itself is not in the repository.

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
