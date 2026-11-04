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
