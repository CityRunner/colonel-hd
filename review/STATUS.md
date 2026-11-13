# Audit status

The customer's brief is `review/TASK.md`. This file is kept current at
every stage so the work can resume in a new session.

## Current stage

**Iteration 2: P1 to P4 done and merged into `main` (`b0fe034`); P5 in progress (2026-11-13).** Version 1.0.0 is set; the tag waits until P5 to P7 and the customer's laptop run are done (customer's choice). Stage 5
is merged into `main` (`b531169`). Iteration 2 runs on branch
`iteration-2`: order in `review/ITERATION2_PLAN.md`, results in
`review/ITERATION2_REPORT.md`. P5: the VM in CI. Stage 5
(started
2026-11-06 on branch `refactor` from `a0422d6`). The plan
(`review/REFACTOR_PLAN.md`) was approved on 2026-11-06 ("1 approve"),
with CI minutes for one Windows draft-PR run and one `quick` run ("4
allowed"). Progress and measured numbers: `review/REFACTOR_REPORT.md`.

- Step 0.1: **done**. `refactor` from `a0422d6`, `review/` brought in;
  two clean release builds (`build/baseline-release`,
  `build/baseline-release-2`) are byte-identical (569 outputs), so S6 can
  use binary identity. MT-32 ROMs are present (mount
  `/home/user/roms:/roms`). The docker daemon was down from the reboot at
  about 06:48 UTC until about 07:25 UTC (restarted by the main session).
- Step 0.2: done (numbers in the report).
- Step 0.3: done except the MT-32 MIDI streams per sound resource and the
  ARM64 (qemu) run of the new tool (see the report).
- Step 0.4: done.
- Step 0.5: done with findings B-45 (script loader) and B-46 (music
  player hang); B-44 (fuzz-save never fuzzed).
- Step 0.6: done (`scripts/check-local.sh`, `tests/README.md`).
- Step 0.7: done, the gate passes on the original code; B-47 (SDL
  revision from git describe) found; S6 uses a fixed SDL_REVISION.
- The two questions after step 0 are answered ("1a 2a", see the
  decisions); 2a is implemented. Next: the main session runs the full
  local checks and pushes; then step 1.
- Step 1 (SPDX headers on the 19 scripts, policy script covers
  `.py`/`.sh`, B-26): **committed** 2026-11-11. Quick checks (format,
  policy, hints) pass; clang-tidy, S2/S3 and S6 for it are run by the
  main session after the commit, and their results still have to be
  added to the step's entry in `review/REFACTOR_REPORT.md`, "Steps".
- Steps 2-13: not started. Notes for step 2 (B-25), read but not acted
  on: `third_party/nuked-opl3/VENDORED.md` and
  `third_party/mt32emu/VENDORED.md` record a SHA-256 per file;
  `third_party/SDL3/VENDORED.md` records none (1,504 tracked files),
  although `THIRD_PARTY.md` says each vendored file has one. Step 2 has
  to decide how the check treats that (B-25 criterion 1) and log it
  (logged as B-48 in step 2; B-49 logged in step 9; next free number: B-50).



Earlier: **stage 4 done** (2026-11-06). Files:
`review/BACKLOG_V2.md` (42 items B-01 … B-42: High 7, Medium 12, Low 23,
no Critical) and `review/REFACTOR_PLAN.md` (stage 5: step 0 safety net,
then 11 behaviour-free steps and a closing step; about 73 hours plus the
game-dependent run times).

| Stage | File | State |
|---|---|---|
| 1a Codebase map | `review/MAP.md` | done |
| 1b Recover requirements | `review/REQUIREMENTS_RECOVERED.md` | done |
| 2 Reconcile | `review/GAPS.md` | done |
| 3 Grilling | `review/QUESTIONS.md` | batch 1 answered ("Defaults"); ended by "Move on to 4" |
| 4 Backlog and plan | `review/BACKLOG_V2.md`, `review/REFACTOR_PLAN.md` | done, plan approved 2026-11-06 |
| 5 Refactoring | `review/REFACTOR_REPORT.md` | in progress: step 0 (safety net) |

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

- **Script patches (G-146), the customer's correction (2026-11-06):** "if
  this patches provided by gog version we need to use them too". The GOG
  release ships the patch files `SCRIPT.052` and `TEXT.052` in the game
  folder; they are already used: the resource manager loads them over the
  map entries, as ScummVM does (R-12, `resource_manager.cpp:117-171`,
  tested in CI by `patch_file_test.cpp`, `resource_manager_test.cpp:95`;
  "Patching SCRIPT.052 - OK" in every run), and identification checks
  them (R-03). **Decided (customer); already satisfied.** ScummVM's 16
  in-memory script patches (R-69: lock-up fixes, copy-protection random
  fix) are not from GOG; they stay as ported (spec §7 asks for them), so
  the game behaves as in ScummVM. Confirmed by the customer (2026-11-06,
  "Patches stays"). **Decided (customer).** G-146 is closed.

- **"Move on to 4" (2026-11-06):** stage 3 ends after batch 1; the Medium
  and Low candidates for batch 2 go into the stage 4 backlog with their
  defaults, for the customer to accept or strike. **Decided (customer).**

- **Stage 4 answers (2026-11-06)**, the customer's words: "1 approve"
  — `review/REFACTOR_PLAN.md` is approved, including the game-dependent
  checks after every commit; stage 5 starts. "4 allowed" — CI minutes for
  one Windows draft-PR run and one `quick` run during stage 5. **Decided
  (customer).** Still open: which of the 23 default items to strike
  ("which items?" — listed to the customer), and Q-03 ("which options?"
  — explained to the customer).

- **Customer's answers, 2026-11-06 (relayed by the main session at the
  start of stage 5):**
  1. Backlog defaults, "2 all approved": every item marked "default —
     accept or strike" in `review/BACKLOG_V2.md` (B-13, B-14, B-17, B-21,
     B-22, B-23's iteration-2 part, B-24, B-27 … B-39) is accepted;
     B-40, B-41, B-42 ("default — strike") are struck. Keeping SDL3's
     Apple back-ends (B-01's sub-default) is accepted with them.
     **Decided (customer).**
  2. Q-03 hint licensing, "1a 2b 3a": (1) the hints were made under the
     customer's personal Claude subscription; (2) the customer did not
     check whether those terms allow publishing the hints under
     GPL-3.0-or-later and takes that risk; (3) showing the customer's own
     game pictures to the model was allowed. Q-03 and B-12 are
     **Decided (customer)**. Plan step 4 writes the "Licensing" section of
     `hints/README.md` with exactly this (labels and geometry only, no
     image data; made with Claude under the customer's personal
     subscription; published under GPL-3.0-or-later at the customer's
     risk, terms not checked; the customer's own game pictures shown to
     the model with permission), not "marked ASSUMED" as the plan's step
     4 row still reads (written before the answer).

- **Project rename, the customer's request (2026-11-06, relayed by the
  main session):** "Additional step - rename repo and all files/mentions:
  sierra -> colonel", then "Agreed to your proposals": (1) the project
  name `sierra-hd`/`sierra_hd`/`SIERRA_HD`/`sierraHd` becomes
  `colonel-hd`/`colonel_hd`/`COLONEL_HD`/`colonelHd` everywhere outside
  `third_party/` (program, ini, saves, image tag, archives, hint format
  id, environment variables, patch identifiers, docs, scripts, CMake,
  workflows); Sierra the company, SCI, `SIERRA.COM` and ScummVM's names
  stay; (2) clean rename, old user files ignored, no migration; (3) the
  GitHub repository rename is the customer's step, the local folder
  stays; (4) one extra step at the end of stage 5. Recorded as B-43 and
  plan step 13. The stage 5 bug range now starts at **B-44** (it was
  B-43). **Decided (customer).**

- **After step 0, the customer's answers (2026-11-06, "1a 2a"):**
  1a: the screen hashes (S4 screens) and the VM hashes (S5) run only for
  steps that change build inputs (compiled source, CMake changing
  compile/link commands, hint files, `tests/golden/` inputs); for docs-,
  CI-, script-comment- and layout-only steps S6 (identical binaries with
  the fixed SDL revision) plus S1-S3 suffice; each step says which parts
  ran and why. 2a: a short known-failures list for the fuzz runner
  (`tests/fuzz/known-failures.txt`, read by `scripts/run-fuzzers.sh`):
  `fuzz-script` (B-45) and `fuzz-sound` (B-46) still run, a failure
  matching the logged bug is reported and does not fail the step, any
  other failure still fails; remove a line when its bug is fixed.
  **Decided (customer).** Recorded in `review/REFACTOR_PLAN.md` (end).

- **Smaller tasks (2026-11-11)**, the customer's words: "Split tasks to
  smaller ones to fit in limits". From step 1 on: one plan step per task
  (steps 5 and 13 split further by area); a task makes its change, runs
  only the quick checks (format, policy, hints), commits and reports
  briefly; the long checks (S1 clang-tidy, S2/S3, fuzz, ARM64, S6, and
  S4/S5 where 1a asks for them) are run by the main session as a plain
  background script (`build/step-checks.sh`, git-ignored; S6 reference
  `build/s6-ref.sha256`, the 42 executables built before step 1), and
  their results are added to the step's entry in `REFACTOR_REPORT.md`
  before the push; one task at a time. **Decided (customer).**

- **Where stage 5 stands (2026-11-12 11:50 UTC), for resuming:** steps
  0-8 done and pushed. Step 9 (`dbd471d`, move to `src/sci-platform`)
  committed with B-49 logged (`297ed21`); its long checks pass (a first
  asan failure was B-49, the re-run passed); S6: 7 executables differ, as
  expected (source paths in assertion messages); game checks: all pass (resources, pictures, sounds,
  screens, VM hashes); step 9 is done. Pushed
  before they finished, to keep the work safe at a usage limit (customer's
  request); if they fail, step 9 is reverted. To resume: run
  `build/step-checks.sh game` (git-ignored helper: pre-push pieces, S6
  against `build/s6-ref.sha256`, the local gate), then record the results
  in REFACTOR_REPORT.md (placeholders LONGCHECKS9/GAMECHECKS9), copy
  `build/s6-now.sha256` to `build/s6-ref.sha256`, and go on with step 10.
  If `build/` is lost, the helper is described in this file's
  "Smaller tasks" entry. Docker: if `docker ps` fails, kill dockerd and
  containerd, remove their pid/sock files, start `setsid nohup dockerd`.

## Open items

- **Stage 4 (at STOP), what the customer is asked:**
  1. Approve `review/REFACTOR_PLAN.md` (steps 0-12), including the full
     suite after every commit with the game-dependent checks (S4, S5) on
     the game copy in this container, and the branch `refactor` from
     `a0422d6` (`origin/main`; the local `main` branch is stale at
     `df81541`).
  2. Accept or strike the 20 items marked "default — accept or strike"
     and the 3 marked "default — strike" in `review/BACKLOG_V2.md`
     (silence = accept the default). The plan uses B-21, B-24, B-23 (stage
     5 part) and keeping SDL3's Apple back-ends (B-01).
  3. Confirm or correct Q-03 (hint licensing, ASSUMED; B-12) before any
     release tag.
  4. Optional: CI minutes for one Windows draft-PR run and one `quick`
     run during stage 5.
- **Stage 4 decisions taken by the auditor (within the decided scope):**
  the layout move (B-02) is a stage 5 step, not iteration 2, because it is
  behaviour-neutral and blocks B-03 (reasons in `review/REFACTOR_PLAN.md`);
  stage 5 restructures no sci-vm/render/vector/audio/app code; `review/`
  files are not rewritten after the move (they cite `a0422d6`); after
  stage 5, `src/app/platform` reads `src/sci-platform` and `reports/`
  reads `docs/reports/`.
- **New facts found in stage 4** (in BACKLOG_V2.md): `.gitignore` ignores
  `analysis-cache/` while the program writes `cache/`, and does not ignore
  `files/`, `saves/` or `*.sav` (B-15); 19 tracked `.py`/`.sh` files lack
  an SPDX header, not 15 (B-26); assertion messages carry source paths
  (`std::source_location`), so file moves change binaries but not outputs
  (B-02).
- **Stage 3, batch 1:** all ten answered on 2026-11-06 ("Defaults"); Q-03
  ASSUMED. Batch 2 was not asked ("Move on to 4"); its candidates are
  backlog items B-13, B-14, B-17, B-21, B-27 … B-31 with their defaults.

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

**Stage 5 (current):**

1. `cd /home/user/sierra-hd && git switch refactor && git log --oneline -8`
   (commits "Refactor step 0.N: ..."; nothing pushed).
2. Read `review/REFACTOR_PLAN.md` (follow it exactly), then
   `review/REFACTOR_REPORT.md`: its "Step 0 progress" says which sub-step
   is done and what is left; its "Steps" has one entry per step 1-13 (what
   changed, checks, results); its "Baselines (before)" holds the numbers.
3. The reference build of the original code is `build/baseline-release`
   (kept for the whole stage; `build/baseline-release-2` is the second
   build of step 0.1). Large outputs stay under `build/` (git-ignored).
4. Docker is shared: never start or stop the daemon; if `docker ps` fails,
   wait with an until-loop.
5. Bugs found go into `review/BACKLOG_V2.md` as B-44 onward, unfixed
   (B-44 … B-47 are taken; the next is B-48).

**Earlier stages (for reference):**

1. `cd /home/user/sierra-hd && git checkout audit && git log --oneline -5`.
2. Read `review/TASK.md` (the brief), this file, then `review/MAP.md`.
3. Continue with the next stage only after the customer's "continue".
   Stage 5 follows `review/REFACTOR_PLAN.md` exactly, on branch
   `refactor` from `a0422d6` (`origin/main`; the local `main` is stale at
   `df81541`); bugs found go into
   `review/BACKLOG_V2.md` as B-44 onward, unfixed.
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

- **Path mapping after step 10 (2026-11-12):** the milestone reports are in
  `docs/reports/` (they were in `reports/`); review images are in the
  git-ignored `docs/reports/images/`. Files in `review/` that cite
  `reports/...` mean the audited commit `a0422d6`.
- **Name mapping after step 13 (2026-11-12, B-43):** the project is now
  `colonel-hd`. Files in `review/` cite the audited commit `a0422d6` and
  keep the old names; read them with this mapping:
  `sierra-hd` → `colonel-hd` (program, CMake project and target,
  `colonel-hd.ini` and its section, saves `colonel-hd-NNN.sav`, the save
  header's program string, the hint format id `colonel-hd-hints`, the
  replay header `colonel-hd replay 1`, release archives),
  `SIERRA_HD_*` → `COLONEL_HD_*` (environment variables, for example
  `COLONEL_HD_MOUNTS`), `sierra_hd` → `colonel_hd` and `sierraHd` →
  `colonelHd` (ScummVM patch files and identifiers), the toolchain image
  `sierra-hd-toolchain:…` → `colonel-hd-toolchain:…` (the tag hash changed
  with the Dockerfile's comment; locally the same image was re-tagged). The
  GitHub repository is `CityRunner/colonel-hd`; the local folder stays
  `/home/user/sierra-hd`. The reference ScummVM was rebuilt from the
  renamed patches in `build/reference-scummvm` (the old build is kept in
  `build/reference-scummvm-sierra`).
