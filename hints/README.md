# Scene-analysis hints

Hints help the vector path (spec section 6.7) where its automatic rules
cannot tell what the artist meant. The main case is a chain of PIC lines
that imitates a curve but is too coarse for the automatic fitting: an
arch, a drape, a barrel. A hint says which lines form the curve, and of
what kind it is.

There is one file per picture, `pic.NNN.json`. The analysis is done
offline, and the files are committed; the program never changes them. They
were frozen at the milestone 3b gate (addendum A1 in
`docs/SPEC_ADDENDA.md`; 2026-10-21). A change is now a reviewed commit like
any other, and changes the golden hashes. Milestone 10 unfreezes them and
freezes them again at its gate (addendum A4). It adds the scene model
(format version 2, below).

## Rules the program applies

Hints are advice. The program applies a hint only where the acceptance
criteria of spec section 6.4 still hold; otherwise the automatic result
stands, and the replacement report (`tools/hd-report --hints hints`) lists
the hint with the reason.

- **Chain hint**: the lines are fitted by the requested kind of curve, at
  most 1 pixel from the lines (the automatic fitting allows 0.5). The
  curve passes through every point where other lines meet the chain. It is
  kept only if:
  - it crosses no other line and encloses no other point, so the faces do
    not change;
  - every pixel the lines own at 1x is still touched by the 1-pixel
    stroke along the curve.
- **Gap hint**: a connector from one end of a line to the nearest point of
  another line, at most 8 pixels away. It is kept only if the number of
  mixed faces and of pixels drawn from 1x blocks does not grow.
- **Regions and symmetry** are information for people and for later
  milestones; the vector path does not use them.
- **Relations** (depth and containment) are read and validated from
  milestone 3a. The vector path uses them from milestone 3b, as evidence
  for drawing order and occlusion (spec section 6.10).
- A file whose `pic_sha256` does not match the picture's data is skipped
  with a message. This happens with another release of the game.

Hints refer to lines by their **command index**: the position of the
line's command in the picture's command stream, as `sci-pic` parses it.
Each parsed command draws at most one line. They never use coordinates,
so they hold for mirrored drawings and overlays too.

## Format

The formal description is `schema.json`. `scripts/check-hints.py` checks
every file against it in CI. The program's reader (`parse_hints` in
sci-vector) applies the same rules and rejects unknown members.

```json
{
  "format": "sierra-hd-hints",
  "version": 1,
  "picture": 44,
  "pic_sha256": "745a7dec953b8deb7cef18f3a3a1f3fc7a04750b439837b34a3dc506861978d8",
  "analyser": "who or what made the hints",
  "regions": [{"id": "rug", "at": [120, 150], "label": "rug", "uncertain": true}],
  "chains": [{"id": "door-arch", "lines": [345, 346, 347], "primitive": "arc", "label": "door arch"}],
  "gaps": [{"id": "shelf", "from": 72, "end": "end", "to": 90}],
  "symmetry": [{"kind": "mirror", "chains": ["arch-left", "arch-right"]}],
  "relations": [{"kind": "in-front-of", "a": "door-arch", "b": "rug"}]
}
```

| Member | Meaning |
|---|---|
| `picture` | The PIC resource number. It must match the file name. |
| `pic_sha256` | SHA-256 of the PIC resource data the hints were made for. |
| `analyser` | Who or what made the hints. |
| `id` | Unique in the file: 1 to 32 characters of `a-z`, `0-9` and `-`. |
| `lines` | Command indices, 2 to 512. A line is in at most one chain. |
| `primitive` | `line`, `arc` (per section between meeting points), `spline` (cubic pieces, smooth at the meeting points) or `ellipse` (a closed chain that nothing meets). |
| `from`, `end`, `to` | A gap from the `start` or `end` point of line `from` to line `to`. |
| `at` | A pixel of the region, in picture coordinates as stored (x 0-319, y 0-189). |
| `label`, `note` | Printable ASCII, at most 200 characters. Labels describe what is drawn, in a few words. |
| `kind`, `a`, `b` (relations) | Depth and containment between two regions or chains of the file: `a` is `in-front-of`, `behind`, or `contains` `b`. Each relation once; never an item with itself. |
| `uncertain` | The analyser was not sure; the customer spot-checks these. Default false. |

The files hold labels and line references only, never image data, so they
contain nothing derived from the game art beyond a description.

## The scene model (version 2)

Milestone 10 (addendum A4, part A, in `docs/SPEC_ADDENDA.md`) describes
each picture as objects made of parts, and parts made of primitives. Two
optional members hold this: `objects` and `repetitions`. They are
information for milestone 10, and the vector path does not use them yet.

**Versions.** Files with the scene model say `"version": 2`. A version 1
file stays valid and means the same as before. It is a version 2 file that
has no `objects` or `repetitions`, and version 1 does not allow them. To
migrate a file, change its version to 2 and nothing else; do this when it
first gains objects. The committed files stay at version 1 until then.

```json
"objects": [{
  "id": "door", "type": "door", "cell": "B3.3-C3.1", "confidence": 600,
  "parts": [
    {"id": "door-jambs", "role": "jamb", "primitives": [
      {"id": "door-jamb-left", "kind": "line", "commands": [43], "points": [[140, 110], [140, 150]]}]},
    {"id": "door-arch", "role": "lintel", "primitives": [
      {"id": "door-arch-curve", "kind": "arc", "commands": [40, 41, 42],
       "points": [[140, 110], [150, 100], [160, 110]], "corners": [{"point": 0, "kind": "rounded"}]}]}],
  "relations": [{"kind": "tangent", "items": ["door-arch-curve", "door-jamb-left"]}],
  "alternatives": [{"id": "door-pointed", "type": "door", "confidence": 400, "note": "pointed arch",
    "parts": [{"id": "door-pointed-top", "role": "lintel", "primitives": [
      {"id": "door-pointed-apex", "kind": "polyline", "commands": [40, 41, 42],
       "points": [[140, 110], [150, 100], [160, 110]], "corners": [{"point": 1, "kind": "sharp"}]}]}]}],
  "decision": {"hypothesis": "door", "by": "customer", "date": "2026-10-30", "cell": "B3.3"}
}],
"repetitions": [{"id": "windows-row", "items": ["window-left", "window-right"]}]
```

`tests/hints/pic.900.json` is a complete synthetic example. It is checked
together with this directory.

| Member | Meaning |
|---|---|
| object `type` | A.2: `window`, `door`, `arch`, `staircase`, `column`, `roof`, `railing`, `fence`, `gate`, `furniture`, `fireplace`, `curtain`, `painting`, `wall`, `floor`, `ceiling`, `building`, `bridge`, `dock`, `well`, `tombstone`, `path`, `foliage`, `tree`, `rock`, `cloud`, `water`, `terrain`, `figure` or `other`. |
| organic types | `foliage`, `tree`, `rock`, `cloud`, `water`, `terrain` and `figure` get no geometric idealisation (A.4). Their primitives are only splines and polylines, and they have no relations and no repetitions. |
| `cell` | Where the object is on the review grid of A4 part B: a cell `B3`, a quarter `B3.2`, or a range `A2-A4` (upper case, no spaces). The rules are those of `tools/common/review_grid.hpp`. |
| `confidence` | Per mille, 0 to 1000. The files hold integers only. |
| part `role` | What the part is to its object: `frame`, `glazing-bar`, `sill`, `step`, `baluster`, `trunk` and others (the list is in `schema.json`), or `other`. |
| primitive `kind` | The intended shape: `line`, `arc`, `ellipse`, `spline` or `polyline` (a polyline has sharp corners). |
| `commands` | The PIC command indices of the lines the primitive replaces, as chains name them. They are never analyser ids. |
| `points` | 1x pixels in stored coordinates, in order: the ends, corners and points the shape passes through. A line has 2, an arc 3 (start, a point on it, end), an ellipse at least 3, a spline or polyline at least 2. |
| `corners` | `sharp`, `rounded` or `step` (an intentional step, kept by A.4) at a point, by its index in `points`. Each point has at most one. |
| `relations` (in an object) | A.3, between parts or primitives of the same hypothesis: `parallel`, `perpendicular` (2 items), `equal-spacing` (3 or more), `symmetry` and `aligned` (these two with `axis` `horizontal` or `vertical`), and `tangent` (one arc and one line primitive). They are not the depth relations of the file. |
| `alternatives` | A.6: up to 8 other hypotheses for the object. Each has an `id`, `type`, `confidence`, `parts` and `relations`. The object's own members are its main hypothesis, named by the object's id. |
| `decision` | A.7: the customer's choice of a hypothesis (`by` is always `customer`), with the date and the cell of the answer. An object with a decision is never asked again. Without one, an object with alternatives is still open. |
| `repetitions` | A.5: objects of one type, or parts of one role, that are reconstructed identically. An item is in at most one repetition. |

All ids are unique in the file, across every kind of item. Command indices
are checked against the picture with the game files by `sci-hint-tool
check`.

## Making hints

`tools/hint-tool` supports the analysis (see its header comment):

1. `sci-hint-tool <game> candidates <n> <directory>` proposes the chains
   of picture n that may be curves the automatic fitting missed. It tries
   each candidate as an arc, a spline and a straight line under the rules
   above, and draws those with an accepted kind, numbered, over the dimmed
   picture. The image is derived from the game art; write it to a
   git-ignored place.
2. Looking at the picture, decide which candidates are curves in the art
   and which are meant to be angular (a roof, a rock, a polygon). Choose
   the kind and write a label. Mark doubtful decisions `uncertain`.
3. `sci-hint-tool <game> check hints` checks the files against the game
   and shows what the vector path does with each one.

`docs/ASSUMPTIONS.md` records how the committed hints were made
(milestone 3a).
