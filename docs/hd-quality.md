# HD background quality: round-trip baseline

The round-trip check of A4 A.6 (`docs/SPEC_ADDENDA.md`), run over every
picture of the game with the repository's hints, on the reconstruction of
milestone 3b (before any milestone 10 change to the rendering). Measured on
2026-10-30 with

    SIERRA_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/hd-report/sci-hd-report /game build/hd-report.md \
        --scale 1 --hints hints --round-trip build/round-trip.txt

## After the scene model steers the fit (2026-11-04)

Measured the same way after "the scene model steers the fit"
(`docs/hd-pipeline.md`, section 6; batch 1 of the customer's answers).
The tables below are the baseline; only pictures with scene-model data
changed:

| Picture | Chains (baseline) | Chains now | Fail | Worst |
|---|---|---|---|---|
| 12 | 227 | 231 | 0 | 1 px, unchanged |
| 58 | 94 | 97 | 0 | 1 px, unchanged |
| 74 | 12 | 13 | 0 | 1 px, unchanged |
| 174 | 14 | 14 | 0 | 1 px, unchanged |

- All: 13212 chains, 13195 pass, 17 fail (the same 17 hinted splines).
  Every other picture's line is identical to the baseline.
- The 13 steered primitives: 1 ellipse (deviation 1: the fanlight of
  picture 12), 6 polylines (all 1: front step noses and cellar bulkhead
  frame of picture 12, three glints of picture 58; the fourth glint is
  already straight), 6 lines (all 0: springers and risers of picture 12,
  a step nose and the opening edge of picture 74). Lines of a steered
  primitive left the automatic chains they were in, which is why picture
  12 (8 steered) has 4 chains more.
- No steered primitive was rejected by the round trip.
- Pixels: 163981 original (was 163572), 17329 added, 17119 missed.

## After batch 2 (2026-11-04)

Measured the same way after the customer's batch 2 answers
(`docs/ASSUMPTIONS.md`, "Milestone 10: batch 2 applied"), with arc
steering and the line check for steered shapes:

| Picture | Chains before | Chains now | Fail | Worst |
|---|---|---|---|---|
| 10 | 181 | 181 | 0 | 1 px |
| 15 | 176 | 177 | 0 | 1 px |
| 20 | 245 | 247 | 1 | 2 px, the same hinted spline (`vine-on-wall-9`) |
| 42 | 79 | 78 | 0 | 1 px |
| 57 | 147 | 146 | 1 | 2 px, the same chain (command 670) |
| 71 | 66 | 65 | 0 | 1 px |

- All: 13212 chains, 13195 pass, 17 fail (the same 17). Pictures 37 and
  61 (decided `keep`) and every other picture are unchanged.
- 26 primitives of batch 2 change the HD and pass the round trip
  (deviation 0 or 1): 14 ellipses, 4 arcs, 8 lines; the other line
  primitives (most bars) are their lines already. 4 fall back to
  the 3b reconstruction because the shape would leave pixels their lines
  own untouched: picture 20 `window-eaves-arch-outer-curve`, picture 42
  `window-curtain-right-arch-outer` and `window-curtain-right-arch-lit`,
  picture 57 `medallion-top-lower`. Two more were tried and dropped
  before commit because the round trip rejected them (deviation 2): the
  red rail's inner line of picture 71 with its tip (command 104) and its
  outer line with its straight tail (command 102).
- Arc steering also applies the two `arc` primitives of picture 12's
  decided tall window (`wl-shl-top`, `wl-shr-top`, the shutter tops;
  deviation 1).
- Acceptance (section 6.4): 88 pictures, worst agreement 99.993%, 0
  below 99%, 0 swaps, 0 untouched line pixels.
- A1 residual totals (sums of the report's columns), before / now:
  overshoots 7071 / 7075, doubled boundaries 2859 / 2859, fragments
  6439 / 6452, breaks 10661 / 10670. The changes are in pictures 10, 15,
  20, 42, 44, 57 and 71 only.
- Smoothing suspects: 3041 / 207 / 311 before, 3038 step / 206 corner /
  299 curve now. Of the batch 2 places only the two `keep` places
  (picture 61 B4.1, rank 1; picture 37 B5.3) and the fallbacks (picture
  57 A2.1, commands 556-561; picture 20 A2.3, the outer arch) are still
  flagged; picture 15's outer arch (commands 1412-1417, A2.2) was not
  part of the batch and stays flagged.

## After the findings after batch 2 (2026-11-04)

Measured the same way after picture 71's tube highlights and hook lines
and picture 12's halo (`docs/ASSUMPTIONS.md`, "Milestone 10: halo,
highlight and hook"):

- Round trip: 13208 chains, 13191 pass, 17 fail (the same 17). Picture
  71: 61 chains (was 65; the newly steered lines left their automatic
  chains), 0 fail, worst 1 px. Picture 12 unchanged (231, 0 fail).
- 8 primitives of picture 71 change the HD: 3 lines (deviation 1) and 5
  arcs (deviation 0 or 1). Picture 12's whole ellipse `halo-oval` falls
  back: round trip within 1 px, but the oval leaves three pixels its
  lines own untouched.
- Acceptance (section 6.4): 88 pictures, worst agreement 99.993%, 0
  below 99%, 0 swaps, 0 untouched line pixels.
- A1 residual totals, before / now: overshoots 7075 / 7075, doubled
  boundaries 2859 / 2859, fragments 6452 / 6451, breaks 10670 / 10672;
  the changes are in picture 71 only (now 22 / 8 / 26 / 34).
- Smoothing suspects: 3038 step / 206 corner / 299 curve before, 3035 /
  206 / 299 now (picture 71: 18 step suspects).

## After the shared boundary, second stage (2026-11-04)

Measured the same way on branch `m10-shared-boundary` (`docs/hd-pipeline.md`,
"The shared boundary, second stage"; the curtain rings and the approved
6.4 deviation included), against the `milestone-10` build:

- Round trip: 13202 chains, 13185 pass, 17 fail (was 13208 / 13191 / 17;
  the same 17 fail). Hinted arcs 82 -> 86 (curtain rings), hinted
  ellipses 15 -> 18 (picture 12's halo and the arches the deviation now
  steers), automatic splines 10685 -> 10672 (lines now steered).
- Acceptance (section 6.4): 88 pictures, worst agreement 99.993%, 0
  failing, 0 swaps, 0 untouched line pixels; 12 line pixels taken within
  the A.6 tolerance under the approved deviation (picture 12: 3, 20: 4,
  42: 4, 57: 1; listed in the report).
- A1 residual totals, before / now: overshoots 7075 / 7079, doubled
  boundaries 2859 / 2861, fragments 6451 / 6460, breaks 10672 / 10750
  (joints the pass no longer closes at the inner ends of steered shapes).
- Smoothing suspects: 3035 step / 206 corner / 299 curve before, 3033 /
  206 / 297 now (pictures 12, 20, 57).
- Steered shapes: 58 (the 47 before; the halo, three arch shapes of
  pictures 20 and 42 and picture 57's upper medallion's lower edge, which
  the deviation lets through; 6 curtain rings): 44 share their boundary,
  14 keep 3b faces (see `docs/ASSUMPTIONS.md`).

## After relation solving (2026-11-04)

Measured on branch `m10-relations` (`docs/hd-pipeline.md`, "Relation
solving"; picture 42's rhombus added) against the `milestone-10` build:

- Round trip: 13208 chains, 13191 pass, 17 fail (was 13202 / 13185 / 17;
  the same 17 fail). New chains: picture 42's two rhombus polylines,
  picture 12's riser feet and fence rails (now steered through solved
  points).
- Acceptance (section 6.4): unchanged; 0 untouched line pixels; the same
  12 pixels taken within the A.6 tolerance (the rhombus needs none).
- A1 residuals, only pictures 12 and 42 change: picture 12 overshoots 177
  -> 178, breaks 235 -> 237; picture 42 fragments 74 -> 75, breaks 132 ->
  134 (the rhombus corners, where the two halves now meet at one point).
- Smoothing suspects: identical (`--smoothing-suspects` output equal).
- Relations: solved in picture 12 (`front-steps` 0.31 px, `picket-fence`
  0.16 px) and 42 (`cabinet-rhombus`, 1.00 px); `cellar-bulkhead` solved
  but steered unsolved (the solved leaf edge lost its shared boundary and
  showed slivers); not solved by rule in 12 `window-left-tall` and 74
  `attic-stairs` (decided over alternatives, as drawn); not solvable
  (items not steered lines) in 12 `window-upper-left`, `-mid`, 58
  `back-window-left`, `-right`; picture 174 and 58's `rose-window` hold as
  drawn.

## The metric

`sci::render::check_round_trip` (`sci/render/round_trip.hpp`), per fitted
chain of the visual scene (a chain of PIC lines replaced by a fitted line,
arc, spline or ellipse, automatically or by a chain hint):

- **Reconstruction**: the strokes of the chain's lines as fitted, before
  the stroke integrity pass, rasterised to 1x: every point of the
  flattened curve snapped to the pixel it lies in, staircase corners
  dropped, consecutive pixels joined by path 1's line rule.
- **Original**: the pixels the chain's lines drew by path 1's line rule.
- **Deviation**: the Chebyshev distance (8-neighbours are 1 apart) from
  every pixel of either set that is not in the other to the nearest pixel
  of the other; the maximum, and the mean over the pixels of both sets.
- **Verdict**: a chain passes at a deviation of at most 1 original pixel
  (`round_trip_tolerance`).

Every pixel of the reconstruction contains a point of the curve, so a
deviation of 2 means the curve itself runs more than one pixel outside
the original pixels: it is not an effect of the rasterisation.

## Totals

| Chains | Deviation 0 | 1 | 2 | more | Fail |
|---|---|---|---|---|---|
| Arcs, automatic | 90 | 387 | 0 | 0 | 0 |
| Arcs, hinted | 4 | 70 | 0 | 0 | 0 |
| Lines, automatic | 219 | 6 | 0 | 0 | 0 |
| Lines, hinted | 0 | 1 | 0 | 0 | 0 |
| Splines, automatic | 6202 | 4515 | 0 | 0 | 0 |
| Splines, hinted | 213 | 1480 | 17 | 0 | 17 |
| **All** | 6728 | 6459 | 17 | 0 | **17** |

- 88 pictures, 13204 fitted chains: 13187 pass, 17 fail.
  No ellipse was fitted.
- The chains' lines drew 163572 pixels; the reconstruction adds 17300
  and misses 17089 (each within one pixel of the other set, except in the
  failing chains).
- **Every failure is a hinted spline** (the hint tolerance is 1 original
  pixel of continuous distance from the chain's centre line, against 0.5
  for automatic fits). No automatic fit fails. All failures deviate by 2
  pixels.
- 15 pictures have a failing chain.

## Per picture

Chains checked, passing and failing; the worst deviation, at its 1x
coordinates (picture area), its quarter of the review grid (A4 part B)
and the command of the line it belongs to.

| Pic | Checked | Pass | Fail | Worst | At | Quarter | Command |
|---|---|---|---|---|---|---|---|
| 1 | 293 | 293 | 0 | 1 px | (190, 7) | A3.2 | 402 |
| 2 | 283 | 281 | 2 | 2 px | (85, 37) | A2.3 | 518 |
| 3 | 240 | 240 | 0 | 1 px | (13, 142) | C1.1 | 1299 |
| 4 | 317 | 316 | 1 | 2 px | (122, 165) | C2.4 | 664 |
| 5 | 314 | 314 | 0 | 1 px | (293, 17) | A5.2 | 2568 |
| 6 | 197 | 197 | 0 | 1 px | (147, 183) | C3.3 | 526 |
| 7 | 343 | 343 | 0 | 1 px | (147, 7) | A3.1 | 2092 |
| 8 | 318 | 318 | 0 | 1 px | (6, 108) | B1.3 | 210 |
| 9 | 312 | 311 | 1 | 2 px | (210, 133) | C4.1 | 2814 |
| 10 | 181 | 181 | 0 | 1 px | (149, 25) | A3.1 | 331 |
| 11 | 81 | 81 | 0 | 1 px | (17, 28) | A1.1 | 424 |
| 12 | 227 | 227 | 0 | 1 px | (185, 21) | A3.2 | 763 |
| 13 | 193 | 192 | 1 | 2 px | (132, 186) | C3.3 | 171 |
| 14 | 309 | 308 | 1 | 2 px | (175, 179) | C3.4 | 964 |
| 15 | 176 | 176 | 0 | 1 px | (77, 85) | B2.1 | 97 |
| 16 | 245 | 245 | 0 | 1 px | (286, 180) | C5.3 | 478 |
| 17 | 274 | 273 | 1 | 2 px | (297, 53) | A5.4 | 2046 |
| 18 | 405 | 404 | 1 | 2 px | (129, 177) | C3.3 | 1617 |
| 20 | 245 | 244 | 1 | 2 px | (141, 113) | B3.3 | 1226 |
| 21 | 289 | 287 | 2 | 2 px | (260, 156) | C5.1 | 655 |
| 22 | 391 | 391 | 0 | 1 px | (260, 182) | C5.3 | 2279 |
| 23 | 299 | 299 | 0 | 1 px | (71, 84) | B2.1 | 1976 |
| 24 | 269 | 269 | 0 | 1 px | (224, 173) | C4.4 | 717 |
| 25 | 343 | 343 | 0 | 1 px | (19, 42) | A1.3 | 2722 |
| 26 | 277 | 277 | 0 | 1 px | (6, 169) | C1.3 | 466 |
| 27 | 311 | 311 | 0 | 1 px | (89, 88) | B2.1 | 708 |
| 28 | 227 | 227 | 0 | 1 px | (125, 160) | C2.4 | 1809 |
| 29 | 306 | 306 | 0 | 1 px | (267, 173) | C5.3 | 272 |
| 30 | 296 | 296 | 0 | 1 px | (93, 172) | C2.3 | 200 |
| 31 | 104 | 104 | 0 | 1 px | (156, 101) | B3.3 | 24 |
| 32 | 80 | 80 | 0 | 1 px | (108, 94) | B2.2 | 115 |
| 33 | 59 | 59 | 0 | 1 px | (164, 44) | A3.4 | 1187 |
| 34 | 83 | 83 | 0 | 1 px | (184, 89) | B3.2 | 91 |
| 35 | 66 | 66 | 0 | 1 px | (258, 100) | B5.3 | 428 |
| 36 | 46 | 46 | 0 | 1 px | (155, 52) | A3.3 | 40 |
| 37 | 59 | 59 | 0 | 1 px | (222, 88) | B4.1 | 218 |
| 38 | 44 | 43 | 1 | 2 px | (181, 68) | B3.2 | 1097 |
| 39 | 26 | 26 | 0 | 1 px | (258, 60) | A5.3 | 293 |
| 40 | 27 | 27 | 0 | 1 px | (61, 60) | A1.4 | 293 |
| 41 | 112 | 112 | 0 | 1 px | (70, 148) | C2.1 | 1008 |
| 42 | 79 | 79 | 0 | 1 px | (143, 29) | A3.1 | 967 |
| 43 | 51 | 51 | 0 | 1 px | (91, 11) | A2.1 | 226 |
| 44 | 49 | 49 | 0 | 1 px | (174, 102) | B3.4 | 58 |
| 45 | 73 | 73 | 0 | 1 px | (273, 139) | C5.1 | 266 |
| 46 | 48 | 48 | 0 | 1 px | (270, 27) | A5.1 | 36 |
| 47 | 56 | 56 | 0 | 1 px | (169, 100) | B3.4 | 362 |
| 48 | 38 | 38 | 0 | 1 px | (244, 65) | B4.2 | 482 |
| 49 | 122 | 122 | 0 | 1 px | (62, 63) | B1.2 | 796 |
| 51 | 207 | 207 | 0 | 1 px | (152, 94) | B3.1 | 545 |
| 52 | 134 | 134 | 0 | 1 px | (40, 100) | B1.4 | 376 |
| 53 | 31 | 31 | 0 | 1 px | (120, 69) | B2.2 | 116 |
| 55 | 0 | 0 | 0 | 0 px | | | |
| 56 | 65 | 65 | 0 | 1 px | (295, 162) | C5.4 | 138 |
| 57 | 147 | 146 | 1 | 2 px | (294, 93) | B5.2 | 670 |
| 58 | 94 | 94 | 0 | 1 px | (127, 144) | C2.2 | 1032 |
| 59 | 74 | 74 | 0 | 1 px | (220, 37) | A4.3 | 188 |
| 61 | 80 | 80 | 0 | 1 px | (124, 165) | C2.4 | 241 |
| 62 | 19 | 19 | 0 | 1 px | (231, 63) | B4.2 | 275 |
| 63 | 69 | 69 | 0 | 1 px | (151, 29) | A3.1 | 387 |
| 64 | 67 | 67 | 0 | 1 px | (125, 71) | B2.2 | 328 |
| 65 | 444 | 444 | 0 | 1 px | (169, 43) | A3.4 | 2270 |
| 66 | 82 | 82 | 0 | 1 px | (39, 108) | B1.4 | 131 |
| 67 | 203 | 202 | 1 | 2 px | (130, 103) | B3.3 | 324 |
| 69 | 80 | 80 | 0 | 1 px | (212, 131) | C4.1 | 43 |
| 70 | 280 | 280 | 0 | 1 px | (93, 9) | A2.1 | 1165 |
| 71 | 66 | 66 | 0 | 1 px | (218, 25) | A4.1 | 98 |
| 73 | 78 | 78 | 0 | 1 px | (122, 29) | A2.2 | 115 |
| 74 | 12 | 12 | 0 | 1 px | (25, 115) | B1.3 | 23 |
| 75 | 74 | 74 | 0 | 1 px | (251, 93) | B4.2 | 245 |
| 76 | 130 | 130 | 0 | 1 px | (60, 79) | B1.2 | 1385 |
| 77 | 85 | 84 | 1 | 2 px | (275, 131) | C5.1 | 545 |
| 78 | 338 | 338 | 0 | 1 px | (73, 146) | C2.1 | 2800 |
| 79 | 231 | 231 | 0 | 1 px | (104, 83) | B2.2 | 639 |
| 80 | 48 | 47 | 1 | 2 px | (161, 138) | C3.2 | 191 |
| 83 | 171 | 171 | 0 | 1 px | (144, 86) | B3.1 | 597 |
| 84 | 4 | 4 | 0 | 1 px | (14, 79) | B1.1 | 275 |
| 85 | 118 | 118 | 0 | 1 px | (248, 43) | A4.4 | 913 |
| 86 | 26 | 26 | 0 | 1 px | (291, 183) | C5.4 | 19 |
| 87 | 156 | 155 | 1 | 2 px | (304, 154) | C5.2 | 623 |
| 88 | 7 | 7 | 0 | 0 px | | | |
| 128 | 188 | 188 | 0 | 1 px | (14, 92) | B1.1 | 262 |
| 162 | 2 | 2 | 0 | 1 px | (148, 95) | B3.3 | 22 |
| 174 | 14 | 14 | 0 | 1 px | (228, 63) | B4.2 | 236 |
| 182 | 55 | 55 | 0 | 1 px | (132, 150) | C3.1 | 214 |
| 282 | 39 | 39 | 0 | 1 px | (228, 55) | A4.4 | 291 |
| 382 | 76 | 76 | 0 | 1 px | (135, 130) | C3.1 | 7 |
| 888 | 27 | 27 | 0 | 1 px | (139, 50) | A3.3 | 183 |
| 992 | 0 | 0 | 0 | 0 px | | | |

## The 20 worst chains

By deviation, then by mean deviation. "Seen" counts the missed pixels that
one of the chain's lines owns at 1x.

1. pic 57: spline (hint `marble-wall-vein-14`), commands 668, 669, 670: max 2 px, mean 0.378 px, 17 original / 20 reconstructed pixels, 8 added, 5 missed (5 seen), worst at (294, 93) B5.2, command 670
2. pic 87: spline (hint `bush-outline-right`), commands 623, 624, 625: max 2 px, mean 0.316 px, 8 original / 11 reconstructed pixels, 4 added, 1 missed (1 seen), worst at (304, 154) C5.2, command 623
3. pic 4: spline (hint `round-plaza-edge-3`), commands 664, 665, 666: max 2 px, mean 0.293 px, 45 original / 47 reconstructed pixels, 14 added, 12 missed (12 seen), worst at (122, 165) C2.4, command 664
4. pic 20: spline (hint `vine-on-wall-9`), commands 1226, 1227, 1228, 1229: max 2 px, mean 0.289 px, 40 original / 43 reconstructed pixels, 13 added, 10 missed (10 seen), worst at (141, 113) B3.3, command 1226
5. pic 9: spline (hint `vine-on-house-wall-2`), commands 2814, 2815, 2816, 2817: max 2 px, mean 0.275 px, 24 original / 27 reconstructed pixels, 8 added, 5 missed (5 seen), worst at (210, 133) C4.1, command 2814
6. pic 18: spline (hint `path-edge`), commands 1617, 1618, 1619, 1620: max 2 px, mean 0.238 px, 64 original / 66 reconstructed pixels, 16 added, 14 missed (14 seen), worst at (129, 177) C3.3, command 1617
7. pic 2: spline (hint `tree-branch`), commands 516, 517, 518: max 2 px, mean 0.222 px, 12 original / 15 reconstructed pixels, 4 added, 1 missed (1 seen), worst at (85, 37) A2.3, command 518
8. pic 80: spline (hint `door-stile-wood-grain-2`), commands 189, 190, 191: max 2 px, mean 0.222 px, 12 original / 15 reconstructed pixels, 4 added, 1 missed (1 seen), worst at (161, 138) C3.2, command 191
9. pic 17: spline (hint `curtain-behind-glass-19`), commands 2043, 2044, 2045, 2046: max 2 px, mean 0.194 px, 17 original / 19 reconstructed pixels, 4 added, 2 missed (2 seen), worst at (297, 53) A5.4, command 2046
10. pic 67: spline (hint `seated-figure-s-arm`), commands 324, 325, 326, 327, 328: max 2 px, mean 0.191 px, 45 original / 49 reconstructed pixels, 10 added, 6 missed (6 seen), worst at (130, 103) B3.3, command 324
11. pic 13: spline (hint `dirt-yard-edge-2`), commands 171, 172, 173: max 2 px, mean 0.185 px, 45 original / 47 reconstructed pixels, 9 added, 7 missed (7 seen), worst at (132, 186) C3.3, command 171
12. pic 2: spline (hint `right-tree-branch-11`), commands 2250, 2251, 2252, 2253: max 2 px, mean 0.184 px, 18 original / 20 reconstructed pixels, 4 added, 2 missed (2 seen), worst at (306, 18) A5.2, command 2253
13. pic 21: spline (hint `dirt-path-edge-4`), commands 655, 656, 657, 658: max 2 px, mean 0.167 px, 74 original / 76 reconstructed pixels, 13 added, 11 missed (11 seen), worst at (260, 156) C5.1, command 655
14. pic 14: spline (hint `puddle-edge-2`), commands 961, 962, 963, 964: max 2 px, mean 0.147 px, 50 original / 52 reconstructed pixels, 8 added, 6 missed (6 seen), worst at (175, 179) C3.4, command 964
15. pic 77: spline (hint `wood-grain-line-10`), commands 545, 546, 547, 548: max 2 px, mean 0.102 px, 43 original / 45 reconstructed pixels, 5 added, 3 missed (2 seen), worst at (275, 131) C5.1, command 545
16. pic 38: spline (hint `curved-stair-rail`), commands 1097, 1098, 1099, 1100, 1101, 1102, 1103: max 2 px, mean 0.070 px, 56 original / 59 reconstructed pixels, 5 added, 2 missed (2 seen), worst at (181, 68) B3.2, command 1097
17. pic 21: spline (hint `oak-trunk-edge-left`), commands 832, 833, 834, 835: max 2 px, mean 0.060 px, 41 original / 43 reconstructed pixels, 3 added, 1 missed (1 seen), worst at (79, 161) C2.3, command 832
18. pic 21: spline (hint `oak-branch-left-5`), commands 874, 875, 876, 877, 878: max 1 px, mean 0.613 px, 31 original / 31 reconstructed pixels, 19 added, 19 missed (19 seen), worst at (30, 6) A1.1, command 876
19. pic 22: spline (hint `garden-bed-edge`), commands 2276, 2277, 2278, 2279, 2280, 2281: max 1 px, mean 0.594 px, 68 original / 70 reconstructed pixels, 42 added, 40 missed (40 seen), worst at (260, 182) C5.3, command 2279
20. pic 24: spline (hint `grass-edge-by-path-2`), commands 716, 717, 718: max 1 px, mean 0.591 px, 22 original / 22 reconstructed pixels, 13 added, 13 missed (13 seen), worst at (224, 173) C4.4, command 717

## Reading the baseline

- The 17 failing chains are reconstructions that A.6 rejects: under it,
  their hints must be refitted or dropped (their lines then fall back to
  the automatic fitting), or decided by the customer (A.7).
- Ranks 18 to 20 pass, with half or more of their pixels one pixel off:
  a curve that runs parallel to the stepped chain, half a pixel aside.
  They pass the tolerance as A.6 states it; a mean limit would catch them.

## Smoothing suspects (2026-11-04)

The defect class the customer found in pilot batch 1 (docs/ASSUMPTIONS.md,
"the customer's answers to pilot batch 1"): the 3b reconstruction smooths an
intentionally sharp or regular shape. `sci-hd-report --smoothing-suspects
<file>` (`sci::render::find_smoothing_suspects`,
`sci/render/smoothing.hpp`) lists the places where that may happen, over
every picture, measured the same way as above:

    SIERRA_HD_MOUNTS=/path/to/game:/game docker/run.sh \
        build/release/tools/hd-report/sci-hd-report /game build/hd-report.md \
        --scale 1 --hints hints --smoothing-suspects build/smoothing.txt

The signals, from what the vector analysis exposes (no rendering change):

- **Step**: the stroke integrity pass (`docs/hd-pipeline.md`, section 5)
  joined the ends of two parallel horizontal or vertical lines of one colour
  one to three pixels apart, with the ends offset along the lines (the
  connector is diagonal: a slope), or clipped a line's end onto a parallel
  line beside it (a bend); both lines at least three pixels long, and a
  pixel between them shows another colour at 1x, so the change shows.
  Changes within 4 pixels of each other form one suspect; score 100 per
  joint, 60 per clip, 40 per stacked line beyond two.
- **Corner**: a joint between two lines of one outline meeting at 60 to
  120 degrees whose ends touch diagonally, where both corner pixels show
  another colour at 1x (the connector bevels the corner); or an automatic
  arc, spline or ellipse that passes at least 0.2 px from a vertex where
  two of its lines (two pixels or longer) turn by more than about 26
  degrees and their neighbours at most half as much. Score 100 x (1 - cos
  of the turn) x the shorter leg (at most 6) / 3.
- **Curve**: a run of lines drawn end to end (followed through any vertex
  where the next line turns least, at most 60 degrees, whatever else meets
  there) that turns one way by at least about 70 degrees, whose vertices
  lie within 0.8 px and whose 1x pixels within 1.5 px of one ellipse
  (least squares, vertices weighted, axes 2 to 160 px, ratio at most 6),
  while the HD strokes leave that ellipse by 0.3 px or more. Score: that
  distance in thousandths of a pixel, times the turn (up to 180 degrees)
  / 180.
- Lines of organic things are left out: scene-model objects of an organic
  type, 3a chain hints whose label names a tree, branch, foliage, rock,
  cloud, water, ground, figure and the like, and lines next to a 3a region
  with such a label. Lines whose shape the scene model steers are left out
  (decided).

**Counts** (current state, with the batch 1 fixes): 3041 step / 207 corner / 311 curve. Per picture
(step / corner / curve):

1: 94/5/3; 2: 11/2/4; 3: 31/1/4; 4: 53/6/6; 5: 29/1/7; 6: 51/1/5; 7: 55/3/3;
8: 25/0/0; 9: 37/4/2; 10: 41/6/10; 11: 81/2/2; 12: 66/6/6; 13: 38/2/7; 14:
43/1/8; 15: 37/10/15; 16: 37/2/8; 17: 49/3/12; 18: 38/2/5; 20: 31/2/6; 21:
19/1/1; 22: 70/37/3; 23: 18/0/0; 24: 29/1/1; 25: 71/5/10; 26: 40/2/1; 27:
59/1/3; 28: 95/8/6; 29: 28/0/1; 30: 36/0/5; 31: 64/1/4; 32: 77/6/0; 33:
25/2/2; 34: 56/1/1; 35: 53/7/6; 36: 39/1/3; 37: 46/6/3; 38: 41/2/2; 39:
3/0/3; 40: 3/0/3; 41: 76/3/5; 42: 38/5/5; 43: 23/2/4; 44: 40/3/4; 45:
60/1/6; 46: 32/2/2; 47: 63/4/1; 48: 33/1/2; 49: 18/3/11; 51: 10/0/4; 52:
53/1/1; 53: 11/0/0; 56: 2/0/3; 57: 15/1/7; 58: 35/3/7; 59: 34/2/3; 61:
31/1/7; 62: 15/0/0; 63: 12/1/6; 64: 20/1/1; 65: 25/1/1; 66: 21/2/11; 67:
51/3/9; 69: 31/2/0; 70: 9/2/12; 71: 22/0/1; 73: 43/7/0; 74: 19/0/0; 75:
44/1/0; 76: 43/1/2; 77: 39/0/0; 78: 79/3/6; 79: 9/0/1; 80: 10/0/1; 83:
31/0/4; 84: 52/0/0; 85: 25/3/5; 86: 13/0/1; 87: 30/2/2; 88: 3/3/0; 128:
59/2/0; 174: 9/0/1; 182: 2/0/0; 282: 6/0/2; 382: 3/0/1; 888: 23/1/1

**The 30 highest scores** (the full list is in the file the option
writes):

    1. pic 61: step score 2280, B4.1 at (194, 74), box (190, 72)-(206, 82), commands 1269, 1271, 1273, 1274, 1275, 1276, 1277, 1278, 1279: 20 joints or clips across stacked lines up to 1 px apart, ends offset up to 1 px
    2. pic 12: step score 1500, A5.3 at (268, 61), box (255, 59)-(274, 71), commands 1705, 1706, 1707, 1708, 1732, 1733, 1734, 1770, 1771, 1772, 1773, 1774, 1775, 1776: 13 joints or clips across stacked lines up to 1 px apart, ends offset up to 7 px
    3. pic 69: step score 1500, C3.3 at (158, 159), box (153, 157)-(160, 169), commands 166, 695, 696, 697, 698, 699, 700, 705, 706: 13 joints or clips across stacked lines up to 1 px apart, ends offset up to 5 px
    4. pic 27: step score 1480, C3.1 at (154, 157), box (152, 155)-(180, 166), commands 1168, 1307, 1355, 1356, 1490, 1492, 1720, 1721, 2173, 2430, 2432, 2451, 2457, 2469: 14 joints or clips across stacked lines up to 1 px apart, ends offset up to 11 px
    5. pic 59: step score 1380, C4.1 at (206, 134), box (200, 132)-(212, 143), commands 84, 92, 93, 262, 263, 268, 272, 312, 632, 633, 641, 642, 643: 11 joints or clips across stacked lines up to 2 px apart, ends offset up to 6 px
    6. pic 67: step score 1320, A3.1 at (133, 26), box (126, 24)-(136, 41), commands 783, 784, 786, 1081, 1082, 1083, 1084, 1085, 1086: 12 joints or clips across stacked lines up to 1 px apart, ends offset up to 36 px
    7. pic 7: curve score 1239, A3.2 at (176, 12), box (146, 5)-(176, 17), commands 2093, 2094, 2095, 2096, 2097, 2098: 6 lines turning about 154 degrees; HD up to 1.445 px off the ellipse, 1x up to 1.325 px
    8. pic 28: step score 1220, B3.1 at (156, 77), box (154, 75)-(166, 87), commands 292, 294, 1223, 1245, 2471, 2648, 2649, 2650, 2651, 3013, 3015, 3018, 3022: 11 joints or clips across stacked lines up to 1 px apart, ends offset up to 31 px
    9. pic 128: step score 1220, C3.1 at (156, 127), box (154, 125)-(166, 137), commands 319, 321, 912, 934, 2072, 2212, 2213, 2214, 2215, 2256, 2258, 2261, 2265: 11 joints or clips across stacked lines up to 1 px apart, ends offset up to 31 px
    10. pic 10: curve score 1208, C4.1 at (203, 129), box (199, 115)-(206, 127), commands 1373, 1374, 1375, 1376: 4 lines turning about 151 degrees; HD up to 1.439 px off the ellipse, 1x up to 1.075 px
    11. pic 28: step score 1180, B3.1 at (151, 94), box (149, 92)-(168, 101), commands 947, 948, 1214, 1215, 1217, 1229, 1230, 1232, 1233, 1235, 1613: 11 joints or clips across stacked lines up to 2 px apart, ends offset up to 6 px
    12. pic 128: step score 1180, C3.1 at (151, 144), box (149, 142)-(168, 151), commands 713, 714, 903, 904, 906, 918, 919, 921, 922, 924, 1302: 11 joints or clips across stacked lines up to 2 px apart, ends offset up to 6 px
    13. pic 10: step score 1160, A1.4 at (49, 61), box (43, 59)-(57, 72), commands 391, 392, 393, 403, 411, 412, 443, 446, 447, 448, 449, 1705: 10 joints or clips across stacked lines up to 1 px apart, ends offset up to 9 px
    14. pic 1: step score 1120, A5.4 at (319, 37), box (311, 35)-(319, 55), commands 658, 756, 814, 1116, 1118, 1272, 1273, 1302, 2387, 2388: 12 joints or clips across stacked lines up to 1 px apart, ends offset up to 11 px
    15. pic 25: curve score 1076, B2.4 at (117, 110), box (100, 101)-(118, 113), commands 610, 611, 612, 613, 614: 5 lines turning about 173 degrees; HD up to 1.123 px off the ellipse, 1x up to 1.458 px
    16. pic 17: curve score 1046, A5.4 at (297, 53), box (294, 39)-(302, 51), commands 2043, 2044, 2045, 2046: 4 lines turning about 149 degrees; HD up to 1.268 px off the ellipse, 1x up to 1.062 px
    17. pic 78: step score 1040, A4.2 at (241, 11), box (237, 9)-(250, 18), commands 415, 416, 417, 418, 1032, 1034, 1036, 1038, 1413: 10 joints or clips across stacked lines up to 1 px apart, ends offset up to 7 px
    18. pic 78: step score 1040, A5.1 at (268, 11), box (259, 9)-(272, 18), commands 415, 416, 417, 418, 1032, 1034, 1037, 1038, 1413: 10 joints or clips across stacked lines up to 1 px apart, ends offset up to 7 px
    19. pic 1: curve score 1011, C5.2 at (308, 145), box (308, 137)-(319, 154), commands 1397, 1407, 1414, 1415, 1416: 5 lines turning about 135 degrees; HD up to 1.348 px off the ellipse, 1x up to 1.335 px
    20. pic 49: curve score 1009, A3.3 at (153, 61), box (151, 61)-(161, 85), commands 97, 98, 99, 100, 101: 5 lines turning about 102 degrees; HD up to 1.780 px off the ellipse, 1x up to 1.214 px
    21. pic 52: step score 1000, B1.2 at (58, 85), box (54, 83)-(60, 95), commands 365, 1487, 1844, 1851, 1852, 1854: 10 joints or clips across stacked lines up to 1 px apart, ends offset up to 22 px
    22. pic 71: step score 1000, C4.2 at (239, 137), box (237, 135)-(246, 149), commands 134, 135, 137, 138, 154, 163, 164, 165, 166: 8 joints or clips across stacked lines up to 2 px apart, ends offset up to 3 px
    23. pic 27: curve score 999, B2.1 at (73, 74), box (73, 68)-(84, 76), commands 1896, 1897, 1898, 1899: 4 lines turning about 120 degrees; HD up to 1.498 px off the ellipse, 1x up to 1.460 px
    24. pic 10: curve score 984, A4.3 at (219, 49), box (195, 35)-(220, 54), commands 485, 486, 487, 488, 489, 490, 491: 7 lines turning about 172 degrees; HD up to 1.031 px off the ellipse, 1x up to 1.443 px
    25. pic 25: step score 980, B5.3 at (264, 103), box (262, 101)-(272, 109), commands 375, 506, 507, 508, 516, 517, 1225, 1226: 9 joints or clips across stacked lines up to 1 px apart, ends offset up to 3 px
    26. pic 42: curve score 969, B1.3 at (21, 97), box (6, 99)-(20, 108), commands 20, 21, 22, 24: 4 lines turning about 114 degrees; HD up to 1.530 px off the ellipse, 1x up to 1.208 px
    27. pic 25: curve score 968, B5.2 at (297, 85), box (297, 74)-(312, 92), commands 2968, 2969, 2970, 2971: 4 lines turning about 135 degrees; HD up to 1.290 px off the ellipse, 1x up to 1.290 px
    28. pic 10: curve score 950, A3.4 at (172, 35), box (144, 20)-(171, 33), commands 1275, 1276, 1277, 1278, 1279, 1280, 1281, 1282: 8 lines turning about 158 degrees; HD up to 1.086 px off the ellipse, 1x up to 0.916 px
    29. pic 57: curve score 946, A2.4 at (99, 49), box (80, 51)-(98, 56), commands 568, 569, 570, 571, 572, 573: 6 lines turning about 105 degrees; HD up to 1.621 px off the ellipse, 1x up to 0.770 px
    30. pic 14: step score 940, C4.2 at (233, 154), box (227, 152)-(235, 167), commands 329, 333, 335, 337, 363, 365, 367, 368, 897: 7 joints or clips across stacked lines up to 1 px apart, ends offset up to 2 px

**Validation on batch 1.** The pre-fix state was measured by running the
same build with a copy of `hints/` in which pictures 12, 58, 74 and 174 have
no `objects` and `repetitions` (the scene model steers nothing there, which
is the 3b reconstruction; the stroke integrity pass only spares ends the
scene model squares). All three customer findings are flagged before the
fix and gone after it:

- 74, C1 (the dark steps): step suspects at (28, 152) commands 192, 203,
  204 (score 300, 2nd of 23 in the picture) and (40, 159) commands 194,
  195, 205, 206 (200, 4th), plus two smaller ones at (53, 166) and (65,
  167); none after the fix.
- 58, A4.4 and B4.2 (the glints): clips of the glints' ends onto the
  window frame beside them (commands 679 and 682 on 471 and 472, five
  suspects of score 60) and the joint 683 / 684 at (238, 66) (score 100);
  none after the fix.
- 12, A1.3-A1.4 (the fanlight): curve commands 123-126 (score 247, 14th
  of 80 in the picture); gone after the fix, as is the bevelled corner of
  the cellar bulkhead frame at (168, 131) (commands 620, 623) that the fix
  squared too.

The other suspects of these pictures are the same before and after (12:
80 to 78, 58: 51 to 45, 74: 23 to 19). Game-wide the batch 1 places rank
between about 540 and 3100 of 3570: the detector finds them (recall), but
its score does not single them out, because joints across stacked lines
are everywhere in the game's art (hatching, dithered shading, foliage
outside the organic labels). It is a list to triage by eye, not a verdict.

**Batch 2.** 21 suspects were looked at in crops; 12 were false positives
(6 organic things the labels miss, 6 where the HD does not look wrong at
review size), 9 were kept: 5 questions (picture 37 stair ends, 61 sofa
cushion, 20 window arch, 57 stained-glass medallions, 71 curved rail) and
4 pictures of round-topped windows drawn as polygons, to be fixed like the
fanlight of batch 1 unless the customer objects (pictures 10, 15, 42, 44).
The batch is `reports/images/milestone-10/batch-2/batch-2.md` (git
ignored, with the crops, sheets and plain images).
