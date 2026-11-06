# Audit status

The customer's brief is `review/TASK.md`. This file is kept current at
every stage so the work can resume in a new session.

## Current stage

**Stage 3, grilling session: batch 1 asked → at STOP** (2026-11-05).
Ten questions Q-01 … Q-10 in `review/QUESTIONS.md` (4 High, 6 Medium).
Waiting for the customer's answers to batch 1.

| Stage | File | State |
|---|---|---|
| 1a Codebase map | `review/MAP.md` | done |
| 1b Recover requirements | `review/REQUIREMENTS_RECOVERED.md` | done |
| 2 Reconcile | `review/GAPS.md` | done |
| 3 Grilling | `review/QUESTIONS.md` | batch 1 asked (Q-01 … Q-10), at STOP |
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

- **Answers after stage 1b (2026-11-05)**, the customer's words:
  1. "continue": stage 2 starts. **Decided (customer).**
  2. 5.25" release: "have no copy, mark untested": acceptance of the
     5.25" release is untested scope (V-01, R-04, R-06). **Decided
     (customer).**
  3. "confirmed": the A1 depth and containment relation hints are dropped
     under answer 4 ("not implemented → dropped"). **Decided (customer).**
  4. "remove the m10 worktrees": the 21 local `m10-*` worktrees and
     branches are removed (2026-11-05). The remote branch `milestone-10`
     is still the customer's to delete on GitHub. **Done.**

- **Answers after stage 2 (2026-11-05)**, the customer's words:
  1. "drop macOS completely": macOS is out of scope (G-07); iteration 2
     removes the macOS build paths, CI job, release job and README steps.
     **Decided (customer).**
  2. "clean up history now": the §14 cleanup pass over the whole history
     (no game resources, resource dumps or game-derived images) is done
     now, outside the audit's read-only stages, as a check of every blob
     ever committed. Any history rewrite it calls for (a force push of
     `main`) is put to the customer before it is done. **Decided
     (customer).**
  3. "we'll publish releases later": releases (G-125) stay; publishing is
     the customer's step later (tags cannot be pushed from this session).
     **Decided (customer).**
  4. "continue": stage 3 starts. **Decided (customer).**

- **Answers to stage 3 batch 1 (2026-11-06)**, the customer's word:
  "Defaults". Q-01, Q-02 and Q-04 to Q-10 take their defaults
  (**Decided (customer)**); Q-03 has no default for its first part
  (which account and terms), so Q-03 is **ASSUMED (unconfirmed)**: the
  customer's own Claude sessions, publication under GPL-3.0-or-later
  acceptable, the legal risk with the customer. All High questions are
  closed (Q-03 as assumed). See `review/QUESTIONS.md`, Answers.

## Open items

- **Stage 3, batch 1 (`review/QUESTIONS.md`), all open:**
  - High: Q-01 interpreter safety net (CI cannot run the game; default:
    hashes of our own VM's outputs locally per commit, ScummVM at start and
    end, synthetic bytecode CI suite as backlog); Q-02 scope of "render ALL
    resources" (default: every picture incl. HD priority map at N = 1-16,
    every view cel, glyph, cursor, scenario/playthrough screen; hashes
    committed, images local); Q-03 hint licensing / model terms (G-24; item
    1 only the customer can answer) and keeping the parser word files and
    the old `.pyc` (default keep, no rewrite); Q-04 measurable goals for
    "optimising" (default: baselines in stage 5, iteration 2 fixes only
    measured misses, one reference-laptop run).
  - Medium: Q-05 edges of answer 4 (G-150; default: PART remainders become
    backlog items, no A4 grid tooling); Q-06 where run-time files live
    (G-122; default: one data folder, `--data` > portable > per-user);
    Q-07 repository layout (default: minimal moves plus git-ignored
    `local/{game,rom,saves}`); Q-08 milestone 9 HD quality final? (default
    yes); Q-09 four missing fuzz targets (default: add in stage 5 step 1);
    Q-10 ini rewrite drops comments (default: fix in iteration 2).
  - Blocking: Q-01, Q-02, Q-09 → stage 5 step 1; Q-04, Q-05, Q-08 → stage 4
    backlog/plan; Q-06, Q-07 → the iteration-2 folder requirement; Q-03 →
    publishing anything new. Stage 3 continues with batch 2 until every
    High question is closed.
  - R-65 (`kDoAvoider`) was not asked: the tested release does not
    reference it (`docs/ASSUMPTIONS.md:1205-1208`).

- **Stage 2 results (`review/GAPS.md`):** 156 G-items (G-01 … G-156)
  covering spec v15 §1-§15 section by section, A1, A2 (withdrawn), A4 and
  the customer's decisions. Classes: FULL 114 (one of them untested
  scope, the 5.25" release), PART 23, DROP 2, DECL 5, CREEP 4, AMB 8.
  Severities: Critical 0, High 3, Medium 6, Low 26, none 121. To carry
  into stage 3 (questions, highest severity first):
  - G-24 (High): the repository is public since 2026-10-22 without a
    recorded history cleanup pass or hint licensing review (§3.4, §14,
    §15). Were they knowingly waived?
  - G-125 (High): no release ever published (no remote tag, no GitHub
    release, the release workflow never ran on a tag); `v0.1.0` and
    `v0.9.0-rc.1` are local tags only. Push them, or drop releases?
  - G-07 (High): macOS in scope but known not to build (`std::jthread`,
    `zoned_time`); no macOS sanitizers or x86-64 target.
  - G-150 (AMB, Medium): the reading of answer 4 and its edge cases
    (G-07, G-24, G-125, G-103).
  - Medium: G-18 (patch files not covered by `.gitignore`/policy), G-44
    (ARCHITECTURE.md stale), G-48 (scripts, sound resources, patch banks,
    configuration not fuzzed), G-122 (AMB: files next to the executable),
    G-123 (performance never measured on reference-class hardware).
  - The local golden-hash check of all pictures was re-run in stage 2 (x86-64): all match
    (`review/GAPS.md`, "Local check run in stage 2"; logs in
    `build/audit-stage2-golden-*.log`).

- Stage 1b results (`review/REQUIREMENTS_RECOVERED.md`): 144 R-items
  (FACT 135, HYPOTHESIS 6, mixed 3; tested CI 95, local only 27,
  untested 15, none/n.a. 7), observations V-01 … V-15. To carry into
  stage 2 (GAPS.md) and stage 3 (questions):
  - V-01: the 5.25" release is said to run without hints, but nothing in
    the code enforces it (only the PIC hash in each hint file); whether
    its PIC data differs is unverified (no 5.25" copy).
  - V-02 / R-36 / R-44: relation (depth/containment) hints of addendum A1
    are parsed but used by no code and present in no hint file; a header
    comment claims they are used.
  - V-04 / R-57: golden hashes miss the HD priority map, N > 10 (and
    3, 4, 6-9), views, fonts, cursors and composed frames; all
    game-dependent goldens are local only.
  - V-06: the repository policy script does not catch SCI0 patch files
    (`SCRIPT.052`, `PATCH.001`, ...) as game data.
  - V-08 (HYP): the program writes ini, cache, files and saves next to the
    executable; a failed ini write is silent.
  - R-65/R-66: `kDoAvoider` is a stub that stops the game; assumed unused
    by the tested game (playthrough ran without it).
  - R-114: the ini is rewritten at exit, dropping unknown keys and user
    comments, persisting a command-line folder.
  - Minor: V-03 (N = 10 in a comment, N goes to 16), V-05 (trigonometry
    comment), V-07, V-10 (no way to see Vm/Resources logs), V-11, V-12,
    V-13 (MAP.md said 87 hint files; 86).

- Stage 1a observations to carry into stages 1b-4 (`review/MAP.md`,
  section 8): CI coverage of sci-vm (38 % lines) and of the app layer;
  the local-only game-dependent checks being the VM's real safety net;
  pixel golden outputs for pictures only; stale docs.
- The remote branch `milestone-10` (customer to delete on GitHub) and the
  local `m10-*` branches/worktrees (customer to confirm removal).
- Confirm the reading of answer 4 (stage 3; G-150).

## How to resume in a new session

1. `cd /home/user/sierra-hd && git checkout audit && git log --oneline -5`.
2. Read `review/TASK.md` (the brief), this file, then `review/MAP.md`.
3. Continue with the next stage only after the customer's "continue".
   Stage 3 asks questions (Q-xx) in batches of 5-10, highest severity
   first, from `review/GAPS.md` (G-xx; its "Items with a severity"
   table), recording answers in `review/QUESTIONS.md` (its "Answers"
   table; "you decide" → the question's default, marked ASSUMED). Batch 2
   starts after the answers to batch 1; its candidates are listed in
   QUESTIONS.md, "Considered for batch 1 and not asked". The original spec
   v15 is not committed; it is in the customer's context.
4. Scratch outputs go under `build/` (git-ignored). The coverage build of
   stage 1a was deleted to save disk; its logs and `llvm-cov` reports are
   in `build/audit-coverage/` (`report-files.txt`: CI suite;
   `report-play.txt`: playthrough). Reproduce with the commands in
   `review/MAP.md`, section 6.4. The toolchain image is
   `sierra-hd-toolchain:fa237f3eeea06223` (`docker/image-tag.sh`); docker
   is shared, do not start or stop the daemon.
