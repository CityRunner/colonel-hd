# Stage 3: questions

Audit of `main` at `a0422d6` (milestone 9), on branch `audit`, 2026-11-05.
Read-only: nothing outside `review/` was changed.

The brief: questions (Q-xx) in batches of 5-10, highest severity first;
for each, what is unclear or questionable, my hypothesis (the default I
would apply) and what changes depending on the answer. If the answer is
"don't know" or "you decide", the default is applied and marked
**ASSUMED (unconfirmed)**. Stage 3 continues until every Critical and High
question is closed.

**Not asked again** (decided, see `review/STATUS.md`): macOS dropped
(G-07); history cleaned now, with no rewrite needed (`review/HISTORY_CLEANUP.md`);
releases published later by the customer (G-125); 5.25" release untested;
A1 relation hints dropped; milestones 10-12 declined; answer 4 rule
"not implemented → dropped" (only its edge cases are asked, Q-05); folder
rearrangement and empty user-file folders are iteration-2 requirements
(only their shape is asked, Q-06, Q-07).

**Severity** follows the brief's scale and is the severity of the item
the question is about. No question is Critical: each has a default that
lets the work go on if the answer is "you decide". The column "Blocks"
says which later step cannot be planned without an answer or the
default.

**IDs:** G- from `review/GAPS.md`, R-/V- from
`review/REQUIREMENTS_RECOVERED.md`, MAP section numbers from
`review/MAP.md`. B- items do not exist yet (stage 4); "backlog" below means
a stage 4 item to be numbered then, "plan step" a step of
`review/REFACTOR_PLAN.md` (stage 4) or of the brief's stage 5.

## Batch 1 (asked 2026-11-05)

| Q | Sev | Refers to | Question (short) | Default if "you decide" | Blocks | Answer |
|---|---|---|---|---|---|---|
| Q-01 | High | G-03, MAP 8.1, MAP 6.3, R-66 | How do we guard the interpreter during refactoring, when CI cannot run the game? | Stage 5: hashes of our own VM's deterministic outputs on the original code, checked locally after every commit; ScummVM comparison at start and end only. CI: synthetic bytecode tests as an iteration-2 backlog item. | stage 4 plan, stage 5 step 1 | *open* |
| Q-02 | High | V-04, R-57, G-54, G-148, MAP 8.3 | What exactly is "render ALL resources", and where do the reference images live? | Every picture (1x maps, HD image and HD priority map at every N = 1-16), every view cel, font glyph and cursor, every scenario/playthrough screen; hashes committed, images local only. | stage 5 step 1 | *open* |
| Q-03 | High | G-24, HISTORY_CLEANUP E, C, D | Hint files: under which account and model terms were they produced, and may they be published under GPL-3.0? Keep the parser word files and the old `.pyc` in history? | Items 1-3 only you can answer (no default for 1). C and D: keep, no rewrite. Record the outcome in `hints/README.md` "Licensing". | publication of iteration 2 | *open* |
| Q-04 | High | G-123, R-92, MAP 8.1 | "Optimising": what should get faster or smaller, by how much, on which machine? | Stage 5 only records baselines. Iteration 2 optimises only measured misses: whole frame ≤ 16.7 ms at N = 10, plus a memory budget set from the baseline; one timing run on a real reference-class laptop. | stage 4 backlog and plan | *open* |
| Q-05 | Medium | G-150, G-103, the 23 PART items | Does answer 4 also drop the missing parts of partly implemented items, and A4 part B's grid tooling? | No: answer 4 drops only wholly unimplemented requirements (G-97, G-98). Each PART item's missing part becomes a backlog item you accept or drop at stage 4. No grid tooling while stage 5 makes no visual change. | stage 4 backlog scope | *open* |
| Q-06 | Medium | G-122, R-125, R-114, V-08 | Where should the ini, saves, cache, game files and ROMs live at run time? | One data folder: `--data DIR`; else the executable's folder if it is portable and writable (today's behaviour); else the per-user folder. `game/`, `rom/`, `saves/` inside it are the defaults. | iteration-2 folder requirement | *open* |
| Q-07 | Medium | answer 5 after stage 1a, G-18, G-29, G-44 | Which repository layout do you expect? (tree below) | Minimal moves: `src/app/platform` → `src/sci-platform`, `reports/` → `docs/reports/`, a git-ignored `local/` with `game/`, `rom/`, `saves/`. Everything else stays. | iteration-2 folder requirement | *open* |
| Q-08 | Medium | G-95, G-96, G-69, G-102 (DECL) | Is the HD picture quality of milestone 9 the final quality for iteration 2? | Yes: HD quality frozen at milestone 9; iteration 2 has no quality work; the `milestone-10` branch may be deleted. | stage 4 backlog scope; deleting `origin/milestone-10` | *open* |
| Q-09 | Medium | G-48, R-138 | Add fuzz targets for scripts, sound resources, patch resources and the ini file? When? | Yes, in stage 5 step 1 (safety net, no behaviour change); crashes found are logged in the backlog, not fixed. | stage 5 step 1 | *open* |
| Q-10 | Medium | R-114, V-08 | Should the ini keep the user's comments and unknown keys, and stop persisting a command-line game folder? | Yes, in iteration 2 (a behaviour change, so not in stage 5): update keys in place, keep the rest, warn in the log when the write fails. | none | *open* |

### Q-01 (High): the interpreter's safety net when CI cannot run the game

**Refers to:** G-03 (the carried risk), MAP 8.1 (sci-vm 38.4 % lines and
`machine.cpp` 2.5 % in CI), MAP 6.3 (the local checks), R-66.
**Blocks:** the stage 4 refactoring plan and stage 5 step 1.

**What is unclear or questionable.** Iteration 2 will refactor sci-vm,
59 % of the first-party code. CI cannot detect a VM regression. The only
real net is local: 39 scenario runs and the 43-checkpoint playthrough
against the patched reference ScummVM (needs your game, the pinned
ScummVM checkout and about 0.8 GB of reference build), save round trips,
and the program playthrough under Xvfb. Three points to challenge:

- **Recording ScummVM's reference traces into CI does not help.** Our side
  of every comparison still has to run the game's scripts, and the game
  files cannot go into CI. The traces would also be game-derived (they hold
  message text and screens), so they could not be committed either.
- **For a behaviour-preserving refactoring, ScummVM is the wrong
  reference.** The reference that matters is our own code before the
  change. Comparing against ScummVM at every commit needs the reference
  build and runs both engines. The run time of the 39 scenarios and of the
  virtual-clock playthrough check is not recorded; a real-time replay would
  take about 94 minutes, `reports/milestone-9.md:156-157`. It also mixes
  in known, accepted differences.
- **What can go into CI** without game data is only what we make
  ourselves: synthetic bytecode built by a test-side assembler, much as
  `PicBuilder` builds synthetic pictures; a small synthetic "game" (one
  script, one picture, one view and a vocabulary, all made by us under the
  GPL); and replays against it.

**Hypothesis (default).**
1. Stage 5, locally: on the original code, run `sci-vm-trace` in
   deterministic mode over the 39 scenarios, the playthrough and the round
   trips. Record a SHA-256 of each output stream (kernel trace, screens, 1x
   maps, actors, parser, layout, OPL/MIDI, state hashes) per scenario in a
   committed file. The file holds hashes only, like
   `tests/golden/pictures.txt`. Generate it twice to prove determinism.
   After every commit, compare the hashes; keep the full traces under
   `build/` to diff a mismatch. Run the ScummVM comparison once before and
   once after stage 5.
2. Iteration 2: a CI suite of synthetic bytecode tests and a synthetic
   mini-game, with a coverage goal for sci-vm in CI (proposed: 70 % of
   lines, `machine.cpp` included). This is a backlog item (High, effort
   large), not a precondition for stage 5.
3. Until item 2 exists, "run the local VM net" is a documented gate
   before any merge to `main` that touches sci-vm.

**What changes depending on the answer.**
- Default accepted: plan step 1 gets "record VM output hashes". It may
  need a hash-only output option in `sci-vm-trace`, which is test tooling
  and no behaviour change. Stage 5 commits are checked in minutes, not
  hours.
- "ScummVM at every commit": stage 5 becomes much slower. The plan then
  batches the VM commits.
- "CI coverage first": the synthetic suite moves from the backlog into the
  plan before any sci-vm refactoring, which delays stage 5 by its effort
  (not estimated yet; stage 4 gives it).
- "No VM refactoring": the plan leaves sci-vm untouched, and item 1 shrinks
  to a smoke check.

### Q-02 (High): what "render ALL resources" covers

**Refers to:** V-04 / R-57 (what the goldens miss), G-54, G-148 (N up to
16), MAP 8.3. **Blocks:** stage 5 step 1.

**What is unclear or questionable.** The brief asks for "render ALL
resources (not a sample) to reference images and compare pixel-by-pixel".
Today's goldens cover the 88 pictures (HD at N = 1, 2, 5 and 10, plus the
original frame) and the 119 sounds, locally. They do not cover:

- the HD priority map;
- N = 3, 4, 6-9 and 11-16;
- the 364 views, 8 fonts and 3 cursors;
- composed game frames, text windows or menus at N.

Open questions:

- **Is N up to 16 justified?** The spec's targets and goldens stop at
  N = 10, but the code accepts 16 (V-03, G-148).
- **Where do the images live?** Reference images are game-derived, so
  they can never be committed and never run in CI.
- **How much space would they take?** Raw RGBA of all 88 pictures at every
  N from 1 to 16 is about 88 x 320 x 200 x 4 x (1² + … + 16²) bytes ≈
  33.7 GB (arithmetic, not measured), so not every image can be kept.

**Hypothesis (default).**
- **Scope**, each rendered twice to prove determinism:
  - every picture: the 1x visual, priority and control maps, and the HD
    image and HD priority map at every N from 1 to 16;
  - every view: every loop and cel at 1x and through the HD sprite path at
    N = 1, 2, 5 and 10;
  - every font glyph and every cursor at 1x and at those N;
  - every screen of the 39 scenarios and of the 43 playthrough
    checkpoints, original and HD at N = 1, 2, 5 and 10;
  - the sounds: the existing 119 OPL hashes plus the MT-32 MIDI streams.
- **Storage:**
  - committed: one SHA-256 per image (hashes only, as today);
  - local only, under `build/` (git-ignored): PNG reference images at
    N = 1, 2, 5 and 10;
  - other N: regenerated from a kept build of the original code when a
    hash differs, then diffed pixel by pixel.
- **Platforms:** x86-64 after every commit; ARM64 under qemu-user once at
  the end.
- **N up to 16** is kept as it is in stage 5, where behaviour must not
  change. Capping N at 10 is an iteration-2 question.

**What changes depending on the answer.** The size of plan step 1:
- whether `sci-golden-hashes` is extended or a new local tool is written;
- the disk space (tens of GB for all N as images, a few GB for four N as
  PNG, not measured);
- the check time per commit.

"Pictures and composed frames only" halves the step. "Cap N at 10" makes
it a backlog item (a behaviour change) and removes N = 11-16 from the
goldens after that change.

### Q-03 (High): hint licensing and model terms (G-24), and the two history items

**Refers to:** G-24, `review/HISTORY_CLEANUP.md` E (items 1-4), C (parser
word files) and D (`.pyc`). **Blocks:** publication of anything new from
iteration 2 (a release included), not the technical work.

**What is unclear or questionable.** The history check found no game data
and no image data in the hint files (E: labels, integers and geometry
only). §15 also asks for the **terms of the model used**, which the code
cannot answer:

1. Under which account and terms were the hint decisions produced? They
   were made by the agent (Claude), reading renders of your pictures:
   `docs/ASSUMPTIONS.md:898-911` and each file's `analyser` field. Was it
   your account or the contractor's? Consumer, commercial or API terms, as
   of October 2026?
2. Do those terms let you publish the outputs under GPL-3.0-or-later? Is
   an AI-generated-content notice expected beyond the `analyser` field?
3. Did the game's licence allow showing renders of its pictures to the
   model? The renders were not committed.
4. Where should the outcome be recorded?

Also, to close the cleanup you asked for:
- **C:** the five `tests/scenarios/parser-words-*.txt` files type 1289 of
  the game's 1295 vocabulary words, as English commands. Keep them?
- **D:** a `.pyc` of our own script, deleted since 2026-10-21, is still
  in history. Keep it?

**Hypothesis (default).**
- Item 1 has no default: only you or the account holder knows.
- Items 2-3: if you say "you decide", I record that the hints are
  published as they are and the terms question stays **open, ASSUMED
  acceptable**, which is a legal risk you carry.
- Item 4: a "Licensing" section in `hints/README.md`, with an entry in
  `docs/ASSUMPTIONS.md`.
- C: keep (single dictionary words as test input, no word classes or
  groups; my judgement in HISTORY_CLEANUP.md C).
- D: keep. No history rewrite.

**What changes depending on the answer.**
- The terms allow publication: a documentation item only. G-24 closes.
- They do not: the hint files have to leave the public repository and its
  history. That means a rewrite with force push (approach in
  HISTORY_CLEANUP.md), and the program then analyses every picture
  without hints. That is a visible HD quality change; every golden hash
  changes; and the frozen hints of milestone 3a are lost. Effort large.
- C is removed: the test generates the parser scenarios at run time from
  your game (the generator exists), plus a history rewrite. Every commit
  hash from `c467bca` on changes, including those cited in `reports/` and
  `review/`.

### Q-04 (High): what "optimising" should target, measurably

**Refers to:** G-123 (never measured on reference-class hardware), R-92,
MAP 8.1. **Blocks:** the stage 4 backlog and the plan.

**What is unclear or questionable.** You asked to "start refactoring and
optimising". The brief's stage 5 forbids behaviour changes and asks only
for performance baselines, so "optimising" has no goal yet. The §10.3
targets are already met with about a 2x margin, but only in the 4-core
development container:

| Measure | Value | Target |
|---|---|---|
| Room entry at N = 10 | ≤ 530 ms | under 1 s |
| First launch, all rooms | 2.8 s | 15 s |
| Changed frames | ≤ 1.4 ms | |
| A whole frame at N = 10, on room entry and mode switch only | 20.7 ms | above 16.7 ms, the one known miss |

Nothing has been measured on the reference machine confirmed at milestone
3 (a 4-core x86-64 laptop of about 2020 with integrated graphics,
`docs/ASSUMPTIONS.md:854-857`), on Windows or on ARM64. Peak memory, build
time and binary size were never recorded. The release binary in
`build/release` is 8.3 MB; whether it is stripped was not checked.

Optimising code that already meets its targets puts the bit-exact
behaviour at risk for no gain you can see.

**Hypothesis (default).**
- Stage 5 records the baselines:
  - room entry per room at N = 1, 2, 5 and 10;
  - the first launch;
  - frame times;
  - peak memory;
  - binary size;
  - clean build time and CI minutes.
- Iteration 2 optimises only measured misses, with these goals:
  - no frame over 16.7 ms at N = 10, room entry and mode switch included;
  - a peak-memory budget set from the baseline (proposed: baseline plus
    10 %, as a regression limit);
  - the §10.3 targets confirmed by one timing run on a real reference-class
    Windows laptop. You run it; the program needs a scripted timing mode
    for it.
- No optimisation of build or CI time unless you name it.

**What changes depending on the answer.**
- "Speed": backlog items on the composition and presentation path, plus a
  performance regression check.
- "Memory" or "binary size": different items. The HD caches and the
  vendored libraries are the likely places (hypothesis, not measured).
- "Code structure": optimising means refactoring only, and the plan
  covers it.
- "Build or CI time": CMake and CI items.
- A real laptop measurement that misses 1 s or 60 FPS would raise G-123
  to High work.

### Q-05 (Medium): the edges of answer 4 ("not implemented → drop")

**Refers to:** G-150, G-103, the 23 PART items of `review/GAPS.md`.
**Blocks:** the scope of the stage 4 backlog.

**What is unclear or questionable.** The decided cases are recorded in
`review/STATUS.md`: macOS, releases and history. Two edges remain.

- **(a) Partly implemented items.** Does answer 4 also drop the missing
  part of a partly implemented requirement? Examples:
  - G-48: four parsers without fuzz targets;
  - G-79: no hint files for pictures 55 and 992;
  - G-41: no switchable log categories;
  - G-69: thin fallbacks only counted;
  - G-36: 20 NOLINT without a reason;
  - G-44: ARCHITECTURE.md stale;
  - G-39: README missing.
- **(b) A4 part B (G-103).** Visual reviews in the audit "use the format
  of A4 part B". Its grid tooling was built only on the declined milestone
  10 and is not on `main`.

**Hypothesis (default).**
- (a) No. Answer 4 drops only requirements with no implementation at all
  (G-97, G-98, already DROP). The missing part of each PART item becomes a
  backlog item with its severity, and you accept or drop it at stage 4.
- (b) Stage 5 must not change any pixel, and the pixel goldens of Q-02
  prove it, so no visual review and no grid tooling are needed. A4 part B
  applies again only if iteration 2 changes the picture output.

**What changes depending on the answer.** With "drop PART remainders
too", the backlog loses about 20 Low/Medium items, including G-44 and
G-48; Q-09 is then moot. With "A4 part B grid now", a tool is ported from
the `milestone-10` branch before that branch is deleted on GitHub.

### Q-06 (Medium): where the program's files live at run time

**Refers to:** G-122 (AMB: "next to the executable"), R-125, R-114, V-08;
answer 5 after stage 1a (empty folders for game, rom, saves).
**Blocks:** the iteration-2 folder requirement (Q-07 depends on it).

**What is unclear or questionable.** The program writes `sierra-hd.ini`,
`cache/`, `files/` and `saves/` next to the executable, and reads `hints/`
from there (`main.cpp:111-115`, `:393-401`, `:1086-1087`, `:1125`,
`:1137`). This causes two problems:
- In an installed copy (Windows `Program Files`, a Linux package) the
  folder is not writable, and a failed ini write is silent. That is a
  hypothesis: it was never tried.
- In a development build the executable sits in `build/<preset>/src/app/`,
  where there are no hints. The local scripts build a run folder with
  symlinks instead (`tools/playthrough/program-run.sh:71-76`).

You asked for empty folders for game files, ROMs and saves. That only
helps if the program looks there by default. Today `game_folder` and
`mt32_rom` have no default.

**Hypothesis (default).** One **data folder** holds `sierra-hd.ini`,
`game/`, `rom/`, `saves/`, `cache/` and `files/`. It is resolved in this
order:
1. `--data DIR`;
2. otherwise the executable's folder, if it holds `sierra-hd.ini` or
   `game/` and is writable (portable mode: today's behaviour and the
   unpacked release archive);
3. otherwise the per-user folder: `%APPDATA%\sierra-hd` on Windows,
   `$XDG_DATA_HOME/sierra-hd` on Linux.

An empty `game_folder` means `game/` in the data folder. MT-32 ROMs are
searched in `rom/`. The release archive ships the empty `game/`, `rom/`
and `saves/`. Hints stay next to the executable.

**What changes depending on the answer.**
- This is a behaviour change, so it is an iteration-2 backlog item, not
  stage 5. It changes `main.cpp`, the config defaults,
  `scripts/package-release.sh`, `docs/USER-README.md` and §10.2's wording
  in `docs/ASSUMPTIONS.md`.
- "Strictly next to the executable": no per-user fallback. Then the
  release archive gets the empty folders and an installed copy stays
  unsupported.
- "Per-user only": no portable mode, and the empty folders in the
  repository only serve development.

### Q-07 (Medium): the "logical layout" of the repository

**Refers to:** answer 5 after stage 1a; G-29 (sci-platform lives inside
app), G-18 (patch files not ignored), G-44. **Blocks:** the iteration-2
folder requirement.

**What is unclear or questionable.** The top level already follows a
common pattern: `src/`, `tools/`, `tests/`, `docs/`, `cmake/`, `docker/`,
`scripts/`, `third_party/` and `hints/`. There are three oddities:
- the library sci-platform sits inside the program's folder
  (`src/app/platform`);
- `reports/` and `review/` sit at the top level next to the code;
- there is no place for your own files.

Every move costs something. It touches CMake, CI, the scripts and hundreds
of path references in `docs/`, `reports/` and `review/`. `git blame`
follows a move only with `--follow`. Large moves give little in return.

**Hypothesis (default): minimal moves.** In the tree below, `←` marks a
move and `+` marks something new:

```text
sierra-hd/
├── src/
│   ├── sci-core/ sci-resource/ sci-pic/ sci-view/ sci-text/
│   ├── sci-vector/ sci-render/ sci-audio/ sci-vm/
│   ├── sci-platform/        ← src/app/platform/
│   └── app/                 the program only
├── tools/                   development tools (unchanged)
├── tests/                   (unchanged)
├── hints/                   shipped with the program (unchanged)
├── docs/
│   ├── formats/
│   └── reports/             ← reports/
├── review/                  the audit (stays at the top while it runs)
├── cmake/ docker/ scripts/ third_party/   (unchanged)
└── local/                   + git-ignored except README.md and .gitkeep files;
    ├── game/                +   your game files
    ├── rom/                 +   MT-32 / CM-32L ROMs
    ├── saves/               +   save games
    └── (sierra-hd.ini, cache/, files/ written by the program)
```

`local/` is the data folder of Q-06 for development. Runs from the build
tree use `--data local`, and the local check scripts use `local/game` and
`local/rom` by default instead of `/home/user/game/...`. One `.gitignore`
rule covers it. The policy script then also rejects any non-placeholder
file under `local/`, which closes G-18 for that folder.

Alternative B: `game/`, `rom/` and `saves/` directly at the top level, as
you listed them. That means three ignore rules and a busier top level.

**What changes depending on the answer.** It sets the size of the move
step: in stage 5 if it changes no behaviour (pure moves), otherwise
iteration 2. With B or "move more" (for example `hints/` → `data/hints/`,
`tools/` grouped), it also changes the release packaging and every doc
path. With "no moves, only `local/`", the step is one commit.

### Q-08 (Medium): is milestone 9's HD quality the final quality?

**Refers to:** G-95 (accepted A1 residue: 7,070 overshoots, 2,859 doubled
boundaries, 6,436 fragments, 7,396 broken joints,
`reports/milestone-3b.md:60-110`), G-96 (depth only measured), G-69
(fallback blocks), G-102 (A4 part A declined). **Blocks:** the scope of
the stage 4 backlog, and deleting `origin/milestone-10`.

**What is unclear or questionable.** This challenges the result, not a
detail. The HD output is the product's purpose (§1). Its known defects
were accepted at 3b on the expectation of later quality work, and that
work (milestone 10) has now been declined. Iteration 2 as asked is
refactoring, optimising and layout, which would ship the milestone 9
quality as final. The work of milestone 10 still exists only on the
remote branch you are about to delete.

**Hypothesis (default).** Intended. HD quality is frozen at milestone 9;
iteration 2 has no quality work; the branch may be deleted.

**What changes depending on the answer.** If you want quality work later:
- keep `origin/milestone-10`, or tag it before deleting;
- the backlog gets a quality item (salvage parts of milestone 10) with its
  own visual review (A4 part B, which then needs the grid tool, Q-05 b);
- the golden hashes would change in iteration 2, after stage 5.

### Q-09 (Medium): fuzz targets for the unfuzzed parsers

**Refers to:** G-48, R-138. **Blocks:** stage 5 step 1 (only if
accepted there).

**What is unclear or questionable.** §5.2 requires a fuzz target for every
parser of untrusted input. Four parsers have none:
- script resources (`script.cpp`);
- sound resources (`midi_parser.cpp`, `music.cpp`);
- the AdLib and MT-32 patch resources;
- the configuration file (`config.cpp`).

Scripts and songs are the largest untrusted input, and sci-vm is the code
to be refactored.

**Hypothesis (default).** Add the four targets in stage 5 step 1, as part
of the safety net. They change no behaviour. Seed their corpora with
synthetic inputs only, never game data. Log any crash in the backlog
without fixing it during stage 5, as the brief requires.

**What changes depending on the answer.** "Iteration 2 instead": a
backlog item (Medium, effort small to medium). "Drop": G-48 stays PART,
recorded as accepted.

### Q-10 (Medium): the ini file loses the user's edits

**Refers to:** R-114 (FACT, `config.cpp:163-190`, `main.cpp:1093`,
`:1133-1134`, `:1171`), V-08. **Blocks:** nothing.

**What is unclear or questionable.** On every normal exit the program
rewrites `sierra-hd.ini` from scratch:
- unknown keys and the user's own comments are dropped;
- a game folder given on the command line is saved as the new default;
- the F12 mode and the viewer's scale are saved;
- a failed write is not reported.

The first two surprise users. Silently losing a hand edit is a small data
loss.

**Hypothesis (default).** In iteration 2 (a behaviour change, so not in
stage 5), the program:
- writes only when a value changed;
- updates the changed keys in place and keeps unknown keys and comments;
- saves the F12 mode and the scale, which are choices made in the
  program;
- does not save a command-line game folder;
- logs a warning when the write fails.

**What changes depending on the answer.** A backlog item (Low to Medium,
effort small) with a test that round-trips a commented file. "Keep as
is": documented in `docs/USER-README.md` instead.

## Considered for batch 1 and not asked

- **R-65 `kDoAvoider` stub.** Answered by evidence: the 4b scan of every
  `callk` found that the tested release references 97 kernel functions,
  and DoAvoider is not among them (`docs/ASSUMPTIONS.md:1205-1208`,
  `reports/milestone-4b.md:18-23`). The playthrough also never called it
  (R-66). ScummVM implements it
  (`engines/sci/engine/kmovement.cpp:401` in the local reference checkout).
  The only risk is another accepted release whose scripts call it, which
  no one has scanned.
  - Proposed backlog item, Low: scan the accepted releases' scripts if a
    copy turns up; otherwise leave the stub.
  - Not a question unless you have such a release.
- **G-123** is folded into Q-04; **G-150** into Q-05; **G-122** into Q-06;
  **G-18** into Q-07.
- Left for batch 2 (Medium or Low):
  - G-44 and G-155: stale docs;
  - G-146: script patches versus "game logic never changed";
  - G-148: the cap on N, unless Q-02's answer settles it;
  - G-41 and V-10: log categories;
  - G-36: NOLINT reasons;
  - G-31: unused dependency edges;
  - G-145: other sound devices;
  - G-79: hint files for pictures 55 and 992.

## Answers

To be filled in from the customer's replies. Each entry gives the
customer's words, the decision, and **Decided (customer)** or **ASSUMED
(unconfirmed)**.

| Q | Customer's words | Decision | State |
|---|---|---|---|
| Q-01 | "Defaults" (2026-11-06) | The batch-1 default: before any change, SHA-256 hashes of every output of our VM (39 scenarios, playthrough, save round trips), generated twice; only hashes committed, checked locally after each commit; ScummVM comparison once at the start and once at the end. Iteration-2 backlog: hand-built bytecode tests and a small home-made game in CI (target 70 % sci-vm lines); until then the local checks are a documented gate before any sci-vm merge. | **Decided (customer)** |
| Q-02 | "Defaults" (2026-11-06) | The batch-1 default: every picture (1x maps, HD image, HD priority map, N = 1-16), every view cel, font glyph and cursor (1x and N = 1, 2, 5, 10), every scenario and checkpoint screen, and the sounds; hashes committed, PNGs kept locally under build/ for N = 1, 2, 5, 10. | **Decided (customer)** |
| Q-03 | "Defaults" (2026-11-06) | (a) No answer to "which account and terms": ASSUMED that the hints were produced in the customer's own Claude sessions under the terms that applied to that account, and that the customer carries the legal risk. (b) ASSUMED acceptable to publish under GPL-3.0-or-later, and showing the customer's game pictures to the model allowed (customer's risk). (c) Recorded in a "Licensing" section of hints/README.md (stage 5). Parser word files and the old .pyc stay; no history rewrite. | **ASSUMED (unconfirmed)** |
| Q-04 | "Defaults" (2026-11-06) | The batch-1 default: stage 5 records baselines only (room entry, first launch, frame times, memory, binary size, build time, CI minutes); iteration 2 fixes measured misses only: no frame over 16.7 ms at N = 10, memory at most baseline +10 %, and one timing run by the customer on a reference-class laptop. | **Decided (customer)** |
| Q-05 | "Defaults" (2026-11-06) | The batch-1 default: answer 4 drops only wholly unimplemented requirements (G-97, G-98); the missing parts of partial items become backlog items for the customer to accept or drop; no A4 part B grid tooling (stage 5 changes no pixels). | **Decided (customer)** |
| Q-06 | "Defaults" (2026-11-06) | The batch-1 default: one data folder, found as --data DIR, else the executable's folder if portable and writable, else the per-user folder; game/, rom/, saves/ inside it, shipped empty in the release archive; iteration 2 (a behaviour change). | **Decided (customer)** |
| Q-07 | "Defaults" (2026-11-06) | The batch-1 default: minimal moves (src/app/platform → src/sci-platform, reports/ → docs/reports/) and a git-ignored local/ with game/, rom/, saves/ kept by placeholder files. | **Decided (customer)** |
| Q-08 | "Defaults" (2026-11-06) | The batch-1 default: milestone 9's HD quality is final; the milestone-10 branch may be deleted (remote: the customer's step on GitHub). | **Decided (customer)** |
| Q-09 | "Defaults" (2026-11-06) | The batch-1 default: add fuzz targets for scripts, sound resources, patch resources and the ini in stage 5 step 1, synthetic inputs only; crashes found are logged, not fixed. | **Decided (customer)** |
| Q-10 | "Defaults" (2026-11-06) | The batch-1 default: in iteration 2 the ini is updated in place (changed keys only), everything else kept, the command-line game folder not saved, a failed write logged as a warning. | **Decided (customer)** |
