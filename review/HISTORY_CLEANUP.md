# History cleanup check (spec §14) and hint-file licensing check (spec §15)

Checked on 2026-11-05 on the `audit` branch, after `git fetch origin`.
This was a read-only check: no history was rewritten and nothing was
pushed. It covers every blob reachable from any ref: 72 local branches,
18 remote branches including `origin/milestone-10` and
`origin/milestone-su`, and the local tags `v0.1.0` and `v0.9.0-rc.1`. Those
two tags were never pushed, and `git ls-remote --tags origin` lists none.

## Verdict

**The history does not need to be rewritten.** No committed blob holds a
game resource, a resource dump, a game-derived image, a ROM, a save file or a
credential. The game-content checks found the following, none of which needs
a rewrite. Items B and C are decisions for the customer.

| # | Finding | Rewrite? |
|---|---|---|
| A | No game resources, dumps, images, ROMs, `.sav` files or secrets in any of the 3909 blobs | no |
| B | Short verbatim quotations of game messages in docs and test comments: 59 distinct spans, 2638 characters in all, the longest 71 characters. The game's text resources total 334,609 bytes in 6259 strings. | no (short quotations) |
| C | `tests/scenarios/parser-words-*.txt` (5 files) are generated from `vocab.000`. Together they type 1289 of the game's 1295 vocabulary words, as commands such as `look at the chest`. No single file holds the vocabulary, and no file holds the word classes or groups. | no (decision for the customer, see C) |
| D | `tools/hint-tool/__pycache__/make_hints.cpython-313.pyc` (blob `1bc577b94d`). It was committed in `f004890` and deleted in `10b33fe`, both on 2026-10-21. It is Python bytecode of our own script and contains the build path `/home/user/sierra-hd`. | optional hygiene only |
| E | The hint files contain only labels, integers or geometry, and text. No image data. | no |

If the customer still wants B, C or D removed from history, see the
[proposed rewrite approach](#proposed-rewrite-approach-only-if-the-customer-asks).

### A. Game resources, resource dumps, ROMs: none

- 121 blobs are binary (contain a NUL byte or are not valid UTF-8):
  - 116 are fuzz-corpus files under `tests/fuzz/corpus/`;
  - 1 is the `.pyc` file of finding D;
  - 4 are in `third_party/`, listed under [Third-party binaries](#third-party-binaries).
- 34 more corpus files are text. All 150 corpus files were checked, text or
  not.
- Every non-`third_party` blob of 32 bytes or more was checked against every
  32-byte window of the game data (method below). This found **no match
  with 8 or more distinct byte values**. Every match was trivial:
  - runs of zeros, or 0x00/0xFF/0x01 padding (up to 64,003 bytes of zeros
    in the synthetic save corpus `tests/fuzz/corpus/save/synthetic-*`);
  - a counting sequence 00 01 02 ... 27 in `tests/fuzz/corpus/pic/patterns-palette`
    (blob `d4bd0ee090`), which also appears in `pic.001` and the CM-32L
    control ROM.
- A second pass over the binary and corpus blobs used **16-byte** windows.
  It found nothing beyond zero padding (at most 11 distinct values) and the
  same counting sequence. One false lead was a 19-byte JSON indentation run
  in `tests/fuzz/corpus/hints/scene` that also appears in
  `goggame-2108578958.info`.
- 83 blobs are binary or corpus files under 32 bytes, all fuzz corpus. They
  were checked for an exact match against the game data. This is
  **low-confidence**, because blobs that short occur by chance:
  - 33 occur somewhere in the game data;
  - 29 of those are 1-8 bytes long and occur in many files, such as
    `ADL.DRV`, `RESOURCE.00x` or `Fingerprints.pdf`. That is chance.
  - The 4 longer ones are runs of a single byte value:
    - `tests/fuzz/corpus/byte_reader/0c158325...` (16 bytes: 15 zeros and a
      newline);
    - `tests/fuzz/corpus/byte_reader/6813b105...` (16 bytes of 0x05);
    - `tests/fuzz/corpus/byte_reader/7e42704a...` (13 bytes of 0xFF);
    - `tests/fuzz/corpus/byte_reader/seek-end` (`@` and 10 zeros).

    None of them is a resource or a dump.
- Byte arrays in source code were checked as well. There are 25 literal
  arrays (`0xNN, ...` or `\xNN`) in 5 files, 808 bytes in all, decoded and
  checked against the game data and the ROMs:
  - `src/sci-pic/src/draw_pic.cpp`: the default EGA palette (40 bytes) matches
    `SCIV.EXE`;
  - `src/sci-pic/src/pattern_tables.hpp`: the texture table (29 bytes in the
    16-byte pass) matches `SCIV.EXE`.

  Both are **interpreter** constants. The file header says they are ported
  from ScummVM's GPL `engines/sci/graphics/picture.cpp`. They are not game
  resources, and they are acceptable under the GPL notice the files carry.
- ROMs: no blob in any part of the repository, `third_party/` included, has
  a non-trivial 32-byte match against the five files in
  `/home/user/roms/roland`. No blob has a ROM size (32 KB, 64 KB, 512 KB or
  1 MB) and no path has a ROM-like name.
- Save files: no path ends in `.sav` or matches a save-file name. The
  `tests/fuzz/corpus/save/synthetic-*` files (about 264 KB each, 6 blobs)
  are synthetic. Apart from zero padding and three short low-entropy runs
  (at most 3 distinct values), nothing in them matches the game.
- No path has a resource-like name (`RESOURCE.*`, `*.SCR`, `*.PIC`,
  `*.V56`, `*.SND`, `*.MAP`, `*.EXE`, `*.DRV`) or an audio or archive
  extension.

### Game-derived images: none

The only blobs with an image signature (PNG, GIF, BMP, JPEG, PPM/PGM, PCX,
ICO, ICNS, RIFF or PDF, whatever the extension) are vendored SDL3/hidapi
files, all from `b02ab2d`:

| Blob | Path | What it is |
|---|---|---|
| `548abe8c1e` (22 KB PNG) | `third_party/SDL3/src/hidapi/documentation/cmake-gui-drop-down.png` | hidapi docs screenshot of CMake GUI |
| `228838ff7c` (77 KB PNG) | `third_party/SDL3/src/hidapi/documentation/cmake-gui-highlights.png` | hidapi docs screenshot of CMake GUI |
| `bb6b7bd5f9` (22 KB ICNS) | `third_party/SDL3/src/hidapi/testgui/TestGUI.app.in/Contents/Resources/Signal11.icns` | hidapi test-GUI app icon |

Outside `third_party/`, the repository has no image file, no SVG, no
`data:image` or base64-encoded PNG/JPEG text, no hex run of 400 or more
bytes and no numeric array of 500 or more entries.
`reports/milestone-3b-pictures.md`, the largest of our own blobs at 2.2 MB,
holds metric tables only.

### B. Game text: short quotations, not dumps

Method: string runs of 40 or more printable characters were taken from every
decompressed `text.*`, `script.*` and `vocab.*` resource. That gives 3048
strings: 3039 from text, 7 from scripts, 2 from vocab. Every text blob
outside `third_party/`, in every version, was then searched for verbatim
matches of 40 or more characters. The 32-byte window pass above gives the
same picture with shorter spans.

| File | Versions with matches | Most in one version | Kind |
|---|---|---|---|
| `docs/playthrough.md` | 19 | 39 spans of 32 bytes or more (1839 chars); 18 messages of 40 chars or more (1412 chars), in a 51 KB document | short quotations that identify a scene or message in walkthrough tables (for example `"I can't go on with this any longer!"`) |
| `tests/playthrough/playthrough.txt` | 7 | 15 spans (599 chars); 6 of 40 chars or more | comments in the replay naming the message each key press closes |
| `tests/scenarios/inventory-and-actions.txt`, `inventory-restore.txt` | 1 each | 3 spans, 43 chars at most | comments |
| `tests/scenarios/grounds.txt`, `outdoors-restore.txt`, `two-songs-restore.txt` | 1 each | 1 span of 52 chars (`The porch lights help to brighten this dreary house.`) | comments |
| `docs/ASSUMPTIONS.md`, `reports/milestone-8.md`, `tests/README.md`, `tests/scenarios/README.md` | many | one string of 32-36 chars (`You cannot restore a game right now.`; `It's real creepy and dark out here!`) | quotation |

Across the whole history, 59 distinct game-text spans are quoted, 2638
characters in all. The game's text resources hold 334,609 bytes in 6259
strings. **No blob is a bulk dump.** No file holds more than 39 spans, and
none holds a whole text resource, a script's strings or a message table.
These are short quotations of single lines, used to identify scenes in
documentation and tests. A rewrite is not needed. The customer may still
prefer paraphrase in future edits of `docs/playthrough.md`.

### C. The parser corpus covers the whole vocabulary (decision for the customer)

`tools/vm-trace/make-parser-scenarios.py` writes
`tests/scenarios/parser-words-{bedroom,downstairs,garden,hallway,kitchen}.txt`.
It reads the vocabulary file that `sci-vm-trace --vocabulary` writes from the
user's own game. That vocabulary file is not committed. The scenario files
were introduced in `c467bca` (2026-10-22, "Milestone 6a: a recorded corpus of
typed commands") and are on `origin/main`.

By design ("together they type every word of the vocabulary at least once"),
the files' `text` lines hold 1289 of the 1295 distinct words of `vocab.000`.
Each word appears inside an English command, for example
`4500 text look at the chest`:

| File | Commands | Vocabulary words |
|---|---|---|
| `parser-words-garden.txt` | 266 | 528 |
| `parser-words-hallway.txt` | 401 | 403 |
| `parser-words-kitchen.txt` | 205 | 251 |
| `parser-words-bedroom.txt` | 199 | 201 |
| `parser-words-downstairs.txt` | 134 | 85 (and 46 tokens outside the vocabulary) |

This is not a dump of the resource. There are no classes, groups,
synonyms or byte layout, and the words are ordinary English words. Still,
together the files reproduce the game's word list almost completely.
**I judge it acceptable**: single dictionary words, used as test input, are
neither a creative expression of the game nor a substitute for it. The
spec's wording, "no resource dumps", is about resources. If the customer
reads it more strictly, both of the following would be needed; one without
the other achieves nothing:

1. regenerate the files at test time from the user's game (the generator
   already exists) and stop committing them;
2. remove the five paths from history (see below).

### D. Hygiene: one committed bytecode file

- Blob `1bc577b94d5101c2a0be8ffed0271fa14e4090a7`, path
  `tools/hint-tool/__pycache__/make_hints.cpython-313.pyc`.
- Added in `f004890` ("Add gap hints at the mixed faces; docs for
  milestone 3a"), deleted in `10b33fe` ("Ignore Python bytecode caches",
  which also adds `__pycache__/` to `.gitignore`).
- It is reachable from `origin/main` and every later remote branch.
- It contains nothing from the game or any third party, only the compiled
  `make_hints.py` and its absolute path `/home/user/sierra-hd/...`.
- It does not need removing. List it in a rewrite only if one is done for
  another reason.

### E. §15 hint files: labels and geometry only

All 143 blobs at 89 paths were parsed as JSON, with no failures:

- `hints/pic.NNN.json` in every commit;
- `hints/schema.json`;
- `tests/hints/pic.900.json`;
- `tests/fuzz/corpus/hints/scene`.

The largest blob is 28.8 KB. Every value is an integer (21,812), a string
(16,465) or a boolean (1533). There are no floats and no nulls. The keys are
only those of the schema:

- `picture`, `pic_sha256`, `analyser`;
- `regions[].{id,at,label,uncertain}`;
- `chains[].{id,lines,primitive,label,uncertain}`;
- `gaps[]`, `symmetry[]`;
- in version 2, `objects[].parts[].primitives[].{kind,points,commands,corners}`,
  `relations`, `repetitions` and `decision.{by,cell,date,hypothesis,note}`.

The longest arrays are short:

| Array | Longest |
|---|---|
| `chains` | 100 entries |
| `chains[].lines` | 17 PIC line indices |
| `primitives[].commands` | 21 |
| `primitives[].points` | 11 points of 2 coordinates |
| `regions[].at` | 2 |

No string is base64 or a data URI. The longest string is the 471-character
description in `hints/schema.json`. **There are no embedded images and no
pixel arrays.** The geometry is line indices into the user's own PIC
resource, points and region seed points. `pic_sha256` is a hash that
identifies the picture; it is not image data.

**The model-terms question.** `docs/ASSUMPTIONS.md:898-911` and each file's
`"analyser": "Claude (AI-assisted review of sci-hint-tool candidates), ..."`
record that the hint decisions (kinds, labels, region labels, symmetry,
uncertain marks) were made by the agent, Claude, reading rendered images of
the user's pictures. `make_hints.py` then turned the decisions into the
files. §15 asks for the terms of the model used. To close that question, the
record needs the following:

1. **Under which account and terms the outputs were produced.** Was it the
   customer's own Claude/Anthropic account, or the contractor's? Was it a
   consumer plan or the commercial/API terms, as of that date? Ownership of
   outputs and any use restrictions come from those terms, not from the
   model. This check cannot answer the question; the customer or account
   holder has to confirm it.
2. **A statement that those terms let the customer publish the outputs under
   the repository's licence (GPL-3.0-or-later)**, and whether any
   attribution or AI-generated-content notice is expected. The `analyser`
   field already provides an attribution.
3. **The inputs given to the model.** It saw renders of the game's pictures,
   which the customer owns as a licensed copy and which were not uploaded to
   the repository. Whether showing them to the model was allowed under the
   game's licence is the customer's call. The hint files themselves carry
   only labels and geometry.
4. **Where the outcome is recorded.** Suggested: a short "Licensing" section
   in `hints/README.md`, with an assumption entry in `docs/ASSUMPTIONS.md`
   pointing to it.

## Proposed rewrite approach (only if the customer asks)

None of the following is required. If the customer decides that C, and
optionally D, must leave the history, use `git filter-repo` on a fresh
mirror clone:

```sh
git clone --mirror https://github.com/CityRunner/sierra-hd.git sierra-hd-clean.git
cd sierra-hd-clean.git
cat > ../remove-paths.txt <<'EOF'
tests/scenarios/parser-words-bedroom.txt
tests/scenarios/parser-words-downstairs.txt
tests/scenarios/parser-words-garden.txt
tests/scenarios/parser-words-hallway.txt
tests/scenarios/parser-words-kitchen.txt
tools/hint-tool/__pycache__/make_hints.cpython-313.pyc
EOF
git filter-repo --invert-paths --paths-from-file ../remove-paths.txt
# verify, then force-push every branch, and ask GitHub support to purge
# cached views and pull-request refs (rewritten history stays reachable from
# PR refs and forks otherwise)
```

Before that, change the tests to generate the parser scenarios at test time.
Otherwise the files come back with the next commit. Every commit hash from
`c467bca` onwards would change. That includes the hashes cited in
`reports/` and `review/`, and the local tags `v0.1.0` and `v0.9.0-rc.1`,
which would have to be re-created.

## Third-party binaries

Listed as required. These are vendored upstream files, and none of them
comes from the game or a ROM:

| Blob | Size | Path |
|---|---|---|
| `548abe8c1e` | 22,316 | `third_party/SDL3/src/hidapi/documentation/cmake-gui-drop-down.png` |
| `228838ff7c` | 77,327 | `third_party/SDL3/src/hidapi/documentation/cmake-gui-highlights.png` |
| `bb6b7bd5f9` | 21,918 | `third_party/SDL3/src/hidapi/testgui/TestGUI.app.in/Contents/Resources/Signal11.icns` |
| `dea12de4ca` | 92 | `third_party/SDL3/src/hidapi/testgui/TestGUI.app.in/Contents/Resources/English.lproj/InfoPlist.strings` (UTF-16 text) |

All four were added in `b02ab2d`. The largest vendored text files are
`d3d12.h` (1.5 MB) and `vulkan_core.h` (1.0 MB), both upstream headers.

## Other risks checked

- **Credentials.** Every blob was scanned, `third_party/` included, for:
  - GitHub tokens (`ghp_`/`gho_`/`ghu_`/`ghs_`/`ghr_`, `github_pat_`);
  - AWS `AKIA` keys, Google `AIza` keys;
  - `sk-ant-` and `sk-` keys, Slack `xox*` tokens, JWTs;
  - PEM private keys;
  - `password`/`secret`/`api_key`/`token` assignments;
  - base64 runs of 200 characters or more.

  Nothing was found in our own files. The only hit, in
  `third_party/SDL3/src/video/windows/SDL_windowsmessagebox.c`
  (`Token='6595b64144ccf1df'`), is the public key token of the Windows
  common-controls manifest, not a secret.
- **Personal data.** No e-mail address appears in our own files. Commit
  authors are `Claude <noreply@anthropic.com>` (465 commits) and the
  customer's GitHub no-reply address (14 commits, committed through GitHub).
  Home paths appear 8 times, only `/home/user` (the build and game paths of
  the container) and `/home/player` (an example). The e-mail addresses in
  `third_party/` are upstream credits.
- **Large blobs.** None of our own blobs is a large binary. The largest are
  `reports/milestone-3b-pictures.md` (2.2 MB, tables), then
  `docs/ASSUMPTIONS.md` and the synthetic save corpus (about 264 KB each).

## Method

1. Ran `git fetch origin`, then `git rev-list --all --parents` (479
   commits). Ran `git ls-tree -r` on every commit to map each blob to all of
   its paths. A blob counts as introduced at a path by a commit when that
   commit has it there and no parent does.
2. Decompressed the game's resources with the repository's tool. The run
   used `docker run` on the toolchain image (`docker/image-tag.sh`), with
   the repository and game mounted read-only, the output in the scratchpad,
   and `build/release/tools/resource-dump/sci-resource-dump /game /out`. It
   produced 1068 resources and 0 failures: 3 cursors, 8 fonts, 6 patches,
   88 pics, 244 scripts, 119 sounds, 227 texts, 364 views and 9 vocabs,
   with `SCRIPT.052` and `TEXT.052` patches applied.
3. Built the game set from every file in the game folder, the decompressed
   resources and, in a separate pass, the five ROM files. Every 32-byte
   window of the set was hashed (8.7 million windows) into a sorted NumPy
   array. Every window of each blob was hashed the same way and looked up.
   Each hit was checked byte for byte, adjacent hits were merged into runs,
   and each run was rated by its number of distinct byte values. Runs with
   fewer than 8 distinct values are padding.
   - A test check confirmed the scan finds slices of `pic.001` and raw
     `RESOURCE.001` placed inside a buffer.
   - A 16-byte pass covered the binary and corpus blobs.
   - Blobs under 32 bytes were checked with exact substring search.
4. Images: signature check of every blob, plus searches for SVG,
   `data:image` and base64 image prefixes, hex runs and numeric arrays.
5. Text: the 40-character window search of section B. Vocabulary coverage
   came from decoding `vocab.000` (1295 words) and comparing with the words
   of each blob.
6. Hints: every hint blob was parsed as JSON and walked. For each one the
   check recorded value types, keys, the longest arrays and any
   base64-looking strings.

The scratch data (resource dump and indexes) was deleted after the check.
Nothing from the game folder or the ROM folder went into the repository.

## Counts

| Item | Count |
|---|---|
| Commits (all refs) | 479 |
| Remote branches | 18 |
| Local branches | 72 |
| Local tags (not on origin) | 2 |
| Blobs | 3909 (2364 own, 1545 only under `third_party/`) |
| Distinct paths | 2357 |
| Bytes of own blobs | 46.1 MB |
| Bytes of `third_party` blobs | 30.5 MB |
| Binary blobs | 121 (116 fuzz corpus, 1 `.pyc`, 4 `third_party`) |
| Fuzz-corpus blobs | 150 |
| Non-trivial matches with game resources or ROMs | 0 (only ported interpreter tables in source, see A) |
| Short blobs with low-confidence exact matches | 33 of 83 (chance; all padding-like) |
| Image blobs | 3 (all `third_party`, none game-derived) |
| Files quoting game text | 11 paths, 59 distinct spans, 2638 chars |
| Bulk text dumps | 0 |
| Vocabulary coverage of the parser scenarios | 1289 / 1295 words (5 files) |
| Hint blobs | 143 at 89 paths, 0 with image data |
| Secrets | 0 |
| `.sav` files | 0 |
| ROM-sized blobs or ROM matches | 0 |
