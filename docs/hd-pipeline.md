# The HD picture pipeline (path 2)

How a PIC resource becomes an HD background (spec sections 6.2 to 6.6).
Path 1 (`sci-pic`) draws the picture at 1x exactly as ScummVM does; path 2
(`sci-vector`, then `sci-render`) turns what path 1 drew into a vector
scene and rasterises it at 320N x 190N. Game logic only ever reads path 1's
maps.

## 1. The trace of path 1

`draw_picture` can record a `DrawTrace`: every line, fill and pattern with
the state it was drawn with (planes, colour pair, priority, control), after
mirroring and in picture coordinates, and for every pixel of the visual and
priority maps the primitive that wrote it last (`visual_owner`,
`priority_owner`). Recording does not change what path 1 draws.

Everything below works per plane: the visual plane (values are EGA colour
pairs) and the priority plane (values 0-15).

## 2. The planar graph

`src/sci-vector/src/topology.cpp` builds an exact arrangement of the
plane's lines.

- **Coordinates** are doubled picture coordinates, so that pixel centres,
  where PIC lines start and end, are integers: pixel (x, y) has its centre
  at (2x + 1, 2y + 1). Line end points are clipped to the screen as path 1
  clips them. The picture frame is the rectangle (0, 0)-(640, 380).
- **Arithmetic** is exact, in `int64_t`: intersections are rational points
  with denominators below 2^24; every product stays below 2^60 and is
  checked (`sci/core/exact.hpp`).
- **Segments** are the plane's lines (dots, lines of one pixel, have none),
  the connectors of section 3, and the frame. All intersections become
  vertices; overlapping segments share edges.
- **Faces** are the cycles of half-edges, the face lying to the right of
  each half-edge on screen. A connected component inside a face gets its
  own outer cycle as a face; no point location is needed, because every face
  takes its value from the pixels it contains (section 4).
- **Labels**: every pixel centre gets its face by an exact scanline walk;
  centres on an edge are marked as such.

## 3. Closing gaps as path 1 closes them

A fill spreads to the four neighbours of a pixel, so two lines whose pixels
touch, even diagonally, stop it, whether or not the lines meet
geometrically. For every line end, the lines whose pixels touch the line's
own pixels within 2 pixels of that end are connected to it, unless the lines
already meet:

1. **Extension**: the line is extended along its own direction to the other
   line, if it hits it within 2 pixels.
2. **Join**: otherwise the end is joined to the other line's nearer end
   point, if that lies within 2 pixels.
3. **Perpendicular**: otherwise a connector runs from the end, square to
   the other line, to its foot on that line, if that lies within 2 pixels.

Dots connect to the lines and dots next to them. A line end in the
outermost pixel row or column connects to the frame, which lies half a
pixel further out. Connectors only close the graph; they are not drawn.

Closing a gap is safe in both directions: if the original fill leaked
through it, the faces on both sides take the same value from the map, so
the leak stays as the player saw it (spec section 6.3, gap rule).

## 4. Values and fallbacks

A face's value comes from the path-1 map: from the *region pixels* whose
centres it contains, that is pixels owned by a fill or left as cleared (not
by a line or brush drawn over them).

- **Uniform**: all region pixels outside the edge band (pixels with a
  neighbour of another value, among their eight) have the face's most
  frequent value. The face is drawn in it.
- **Mixed**: they disagree. The face falls back to 1x blocks: each point
  takes the value of the 1x pixel it lies in (for line and brush pixels,
  the most frequent value of their region neighbours).
- **Thin**: the face contains no region pixel (slivers along lines). It
  falls back to blocks as well.

## 5. Layers

Above the faces, in drawing order:

- **Strokes**: every visible line (one owning at least one pixel at the
  end) is a stroke 1 original pixel wide: all points within 0.5 of its
  centre line, with round ends. Lines that are fully overdrawn are left out.
- **Brushes**: a solid circle pattern is the ellipse inscribed in its box
  (size 1: below), a solid rectangle pattern the box itself
  (`sci/pic/geometry.hpp` gives the box path 1 uses).
- **The smallest circle brush** (size 1, milestone 10, batch 3): its
  bitmap is a plus of 5 pixels in the left 3 x 3 pixels of its 4 x 3 box,
  a small diamond at 1x. It is drawn as that diamond, |u| + |v| <= 1 with
  its corners at the plus's outer pixel edges (`Ellipse::diamond`), not as
  the larger oval inscribed in the box (picture 45's wallpaper ornament;
  the customer's finding of 2026-11-05). Larger circles keep the ellipse.
  The scene format is version 2 (the flag is stored).
- **Blocks**: a textured pattern is drawn as the 1x pixels it owns at the
  end, scaled N x N.

### Stroke integrity (milestone 3b, spec section 6.10)

After the layers are built, the visual plane's line strokes pass through
`apply_stroke_integrity` (`sci-vector/src/stroke_integrity.cpp`). Its
evidence is what path 1 shows: the 1x owner of every pixel (what the player
saw) and the pixels every line drew. The priority plane is not changed. In
order:

1. **Spurs**: stroke points past the line's own end pixels (a fitted edge
   that ran out to a junction on a contour and back) are removed.
2. **Ends on a contour**: where a line's end pixel is seen and touches
   another line that passes there and is seen, and no end of that line is
   next to it, the end is moved to the contour's centreline and cut along
   the contour's near edge (`Cut`: a half-plane removed from the stroke).
   The contour must be within 2 pixels across and the end may move at most
   3 pixels along the line. An end beside the contour, pointing along or
   away from it, gets a short connector to the contour's nearest point
   instead, cut the same way. Ends that drew over the contour, or whose
   pixel lies on the contour's stroke, are kept as the player saw them.
   A line of a steered shape the customer decided stays on top (milestone
   10, picture 12's halo, "halo must be on top"): where a line's end
   pixel touches such a shape next to one of its inner joints (where two
   of its lines meet; the shape runs on there, so it is not a line end)
   and the end pixel's centre lies under the shape's stroke, the shape is
   the contour, and the end is cut at the stroke's near edge, on the side
   of the line's other end, although the player saw the end pixel there;
   points of a fitted end that run on past that edge go with it. Only
   then: ends that the earlier rule clipped, joined or kept elsewhere are
   unchanged. The report gives the evidence "the line ends on a shape the
   customer decided, which stays on top".

3. **Joints**: two seen line ends on touching pixels meet: the earlier line
   gets a connector to the later line's end (connectors are kept apart
   from the line's own strokes). An end next to the other line's end gets a
   connector to its nearest point, cut at its near edge.
4. **Runs not seen**: along the pixels a line drew where the player saw
   another object, half a pixel each side of their centres is removed from
   the line's strokes. A boundary drawn twice therefore shows once, in the
   colour seen at 1x.

Every change is recorded with its evidence (`SceneStats::integrity_changes`)
and listed by `tools/hd-report`. The CMake option
`SCI_WITHOUT_STROKE_INTEGRITY` (development only) builds scenes without
the pass, to measure the baseline.

## 6. Curves

Chains of short lines that imitate curves are fitted
(`src/sci-vector/src/fitting.cpp`, `curves.cpp`).

- **Chains** run through vertices where exactly one other usable edge
  continues the chain and the direction turns by at most 50 degrees. Usable
  edges belong to visible lines up to 12 pixels long (in either axis) that
  were not extended to close a gap. Where other edges meet a chain, the
  vertex is *pinned*: the curve passes through it exactly.
- **Fits**, in order of preference, all keeping the chain's end points:
  a straight line; a circular arc (closed-form algebraic least squares with
  the centre on the chord's bisector); a cubic spline (Schneider's least
  squares with Newton reparametrisation), split at the middle where one
  piece does not fit; for closed chains that nothing pins, an axis-aligned
  ellipse. Pinned chains are fitted section by section with cubics that
  share tangents at the pins, so the curve stays smooth across them.
- **Acceptance of a fit**: the curve stays within 0.5 original pixels of the
  chain both ways, crosses no other edge, and encloses no other vertex
  between itself and the chain. The faces therefore stay the same; the
  curve replaces the chain's edges for both faces and carries the strokes of
  the chain's lines.
- **Arithmetic**: only +, -, *, / and sqrt (spec section 5.3). Curves are
  flattened into chords of at most 0.25 original pixels. Arcs flatter than
  a radius of 1e5 pixels count as straight.

### Hints (milestone 3a)

`build_scene` takes the hint files of the pictures in the trace
(`sci/vector/hints.hpp`; the format is in `hints/README.md`). Hints name
lines by their command index.

- **Chain hints**: the lines of a hint chain only with each other, at any
  turn and any length. `fit_curve_as` fits each section between pinned
  vertices with the requested kind (an ellipse only for an unpinned closed
  chain) within 1 original pixel. The geometric acceptance above applies.
  After the strokes are built, every pixel the hinted lines own at 1x must
  still be touched by the stroke (the line check). A hint that fails is
  dropped, and the curves are fitted again without it, so its lines fall
  back to the automatic fitting.
- **Gap hints**: a connector from a line's end to the nearest point of
  another line, at most 8 pixels away. The faces are built with all gap
  hints; if that adds mixed faces or 1x-block pixels, the hints are tried
  one at a time and only those that do not are kept.
- Every hint used or left out is listed in `SceneStats` (`applied_hints`,
  `rejected_hints`) and in the replacement report.

### The scene model steers the fit (milestone 10)

The scene model of the hint files (A4 A.1; `hints/README.md`) says what
an object's lines are meant to be. `steer_by_scene_model` in
`build_scene.cpp` (visual plane only) applies it after the curves above,
with `src/sci-vector/src/scene_model.cpp`:

- **Which objects.** The hypothesis an object follows (A.7): the decided
  one; the main one of an object without alternatives (certain). An open
  object (alternatives, no decision) keeps the reconstruction above.
  Organic objects (A.4) are never steered. Repetitions are not rendered.
- **Which primitives.** `line`, `polyline`, `arc`, open `ellipse` and
  whole `ellipse` (below), whose
  commands draw lines in this plane that form one path in the
  primitive's order: each starts on or next to the pixel where the
  previous one ends (either direction), and no two consecutive lines run
  side by side (more than two pixels next to each other: a bar drawn two
  lines wide, stacked treads). Splines keep the fit above.
- **The shape.** `line`: one straight segment between the path's ends.
  `polyline`: straight segments meeting at its inner points, each at the
  nearest line end within one pixel (a `rounded` corner leaves the
  primitive to the fit above). Open `ellipse`: half of an ellipse with the
  path's ends as a diameter, the conic about their midpoint fitted by
  weighted least squares to the ends, the anchor points and the lines'
  pixel centres, and drawn through its conjugate diameters (+, -, *, /
  and sqrt only; chords of at most 0.25 pixel). `arc` (batch 2): a
  circle x^2 + y^2 + Dx + Ey + F = 0 fitted by weighted least squares to
  the path's ends (strongly), the anchor points and the lines' pixel
  centres, followed from the start in the direction the first line
  leaves it to the end, through quarter turns, so it may turn by more
  than half a circle (a hook), and flattened the same way. Whole
  `ellipse` (first and last point equal; after batch 2): the conic
  A x^2 + B xy + C y^2 + D x + E y = 1 about the centroid of the lines'
  pixel centres, fitted by weighted least squares to the anchor points
  and those pixel centres (5 x 5 normal equations, Gaussian elimination),
  must be an ellipse; each line's piece is the part of it between the
  line's ends projected radially onto it, the shorter way round, two ends
  on touching pixels meeting halfway. The lines need not form one path:
  where they leave a gap (another object bounds the area there) the gap
  stays. The round trip and the line check below apply unchanged.
  An open `ellipse` or an `arc` whose lines do not form one path may
  still be steered if they do once consecutive ends may be two pixels
  apart (one pixel between them, which another line drew over): the shape
  is fitted as for one path, cut at both ends of such a gap (each
  projected onto it), and the part between stays open, as a whole
  ellipse leaves its gaps (group A follow-ups; picture 16's inner
  fanlight edges). A one-pixel command (a dot) of such a primitive keeps
  its own stroke and is not a line of the shape; a gap is also closed by
  a run of the primitive's dots, each at most two pixels from the next.
- **The strokes.** The shape is cut where consecutive lines meet (halfway
  between their end pixels, or at a polyline corner within one pixel of
  that), and each line's stroke becomes its piece. The faces keep the
  fit above (a face beside a steered curve therefore keeps its old
  boundary, which can show beside the stroke; see `docs/ASSUMPTIONS.md`,
  "Milestone 10: halo, highlight and hook").
- **Acceptance.** The pieces, rasterised back to 1x as the round-trip
  check does (`sci/vector/raster_1x.hpp`, shared with
  `sci/render/round_trip.hpp`), must lie within 1 original pixel of the
  lines' pixels (A.6). Otherwise nothing changes, and the replacement
  report lists the primitive with the deviation ("the 3b reconstruction
  stays"). As for a chain hint, every pixel each line owns at 1x must
  still be touched by its piece (the line check of section 8); otherwise
  the 3b reconstruction stays too ("the shape misses pixels its lines
  own"). A primitive that changes nothing is not recorded; one that
  does is listed as applied, and is a chain of the round-trip check of
  kind `line`, `polyline`, `arc` or `ellipse`, named after the primitive.
- **A wider tolerance the customer approved** (`tolerance` of a
  primitive, `hints/README.md`; at most 2 pixels, only where the customer
  decided the hypothesis): the shape is also used if its 1x raster lies
  within the tolerance rounded up (2) and, measured as A.6 measures
  (Chebyshev distance), every pixel centre of its lines lies within the
  tolerance of the shape and every point of the shape within it of the
  lines (each line as the polyline through its pixel centres). A half
  ellipse that the least-squares fit leaves outside is fitted once more
  towards the smallest largest deviation (40 rounds of Lawson's
  reweighting of the pixels; the ends stay). The steering's line check and `check_acceptance`
  then take a line pixel within 2 pixels (Chebyshev, of the shape's 1x
  raster) as tolerated (`SceneStats::widened_lines`), and every such pixel
  is listed as before; the replacement report says "within the customer's
  tolerance of 1.500 px".
- **Step corners** (`step` in a polyline's `corners`): the end of a
  horizontal or vertical line of the primitive at that pixel runs on to
  the pixel's outer edge and is cut there, square to the line. Stacked
  lines then cover exactly their pixels: horizontal treads, vertical
  risers, sharp corners. The stroke integrity pass (section 5) leaves such
  an end as it is: no joint, connector or contour clip at it, from it or
  to it. Without this, the pass joined the ends of stacked treads on
  diagonally touching pixels and clipped them onto the line above, which
  drew a slope and split the lit area of the attic steps (picture 74, C1).

### The shared boundary (milestone 10, design)

Spec section 6.6: "a fitted curve that replaces a chain of edges is
shared by both adjacent faces". Before this step the scene model steered
only the strokes; the faces kept the 3b fit (`docs/ASSUMPTIONS.md`,
"Milestone 10: the design limit found after batch 2"). Design:

- **Where the steered curve enters.** At the same place the 3a chain
  hints act: after `fit_with_hints`, before the scan edges are built. A
  steered primitive that passes its checks (round trip, line check)
  gives each of its lines a piece; that piece becomes the geometry of
  every arrangement edge of the line (`sci-vector/src/shared_boundary`),
  replacing the line's straight edges or 3b fitted pieces. The faces keep
  their labels and values (taken from the 1x pixels, section 4); only
  where their boundary runs changes.
- **Vertices move onto the curve.** A 3a hint curve passes through every
  pinned vertex; a steered shape does not (that is what makes it straight
  or oval). So every vertex on a steered line is moved onto its piece: a
  line end to the piece's end; a vertex where one other straight line
  meets or crosses, along that line to where it meets the piece (it stays
  straight); otherwise to the nearest point of the piece. Every other
  edge at a moved vertex (other lines, gap connectors, 3b pieces) ends at
  the new point. A vertex keeps one position: two steered shapes that
  disagree on it are a conflict, and the later one falls back.
- **The partition stays valid.** The moved geometry must cross no other
  edge (apart from at shared vertices), the region between a moved edge's
  old and new course must contain no other vertex, and the order of the
  pieces along each line must follow the vertices. Otherwise the shape
  falls back.
- **Fallback, per shape.** A shape that fails the round trip or the line
  check is removed completely (3b strokes and faces). A shape that passes
  them but that the partition cannot take keeps its steered stroke while
  its edges keep the 3b geometry exactly (nothing of the attempt is
  kept), and the replacement report lists it ("the faces keep the 3b
  fit: the shared boundary fails (...)"); see `docs/ASSUMPTIONS.md`,
  "Milestone 10: the shared boundary". Pictures without steered shapes
  are built exactly as before.
- **Strokes** are the pieces, as before; the stroke integrity pass and
  the step corners are unchanged. The line check stays as section 6.4's
  code measures it (`check_acceptance`: a line pixel counts only if its
  own line's stroke touches it; a face covering it does not), so it is
  not loosened for steered shapes.

### The shared boundary, second stage (milestone 10)

- **Shapes of one object are placed jointly** (one call, so vertices they
  share move together); if the partition does not take them so, each is
  tried on its own, in order, as before.
- **A course that leaves a vertex beside a moved course** (an arch
  springing from its jamb, two curves leaving one vertex at almost the
  same direction) and crosses it by a hair is pushed just onto its side
  (points at most 0.25 original pixels off, 1e-6 beyond; the far vertex
  stays; the side is the one it lay on before). The validity checks then
  run unchanged.
- **A straight shape the partition cannot take by moving vertices**
  (a `line` primitive whose ends are pixel centres) enters the
  arrangement itself: its lines' segments are replaced by one segment
  between the path's ends, the gap connectors at the lines' inner ends
  go, and the plane is built again (visual plane, at most once). A shape
  is kept only if the partition gets no more mixed faces or 1x-block
  pixels (the rule of gap hints); such a shape needs no vertex moved
  ("shared by the faces (one segment of the partition)"). This is what
  makes picture 71's tube highlights straight bands.
- **The inner ends of a steered shape** (a line end on or next to the end
  of another line of the same shape) are fixed for the stroke integrity
  pass, like step corners: no joint, connector or clip at them. Joints
  there drew short stubs sideways out of the smooth shape (the "teeth"
  of picture 71's tube highlights).
- **The 6.4 line rule for shapes the customer decided** (approved
  deviation; `docs/ASSUMPTIONS.md`, "Milestone 10: the customer's
  decisions on the halo and the curtain rings"): a pixel of such a
  shape's lines that its stroke does not touch still counts if the shape,
  rasterised back to 1x as the round-trip check does, has a pixel at
  Chebyshev distance at most 1 from it (`within_round_trip_tolerance`,
  `sci/vector/raster_1x.hpp`): in the steering's line check and in
  `check_acceptance`, which lists every such pixel (`sci-hd-report`,
  "Line pixels within the A.6 tolerance"). Automatic fits and analyser
  hypotheses keep the full rule.
- **Not done:** re-fitting a neighbouring 3b chain whose end vertex moved
  (the remaining failures are not of that kind; see `docs/ASSUMPTIONS.md`,
  "Milestone 10: the shared boundary, second stage"); curved shapes do not
  enter the arrangement (its segments have integer end points).

### Curved shapes in the arrangement (milestone 10, design)

The third stage of the shared boundary. A curved steered shape that passes
its round trip and line check but whose faces cannot follow by moving
vertices (the course crosses a connector at a springer, changes the order
at a vertex, passes over a nearby line end) enters the arrangement itself,
as the straight shapes of the second stage do, and the plane is built
again. The exact arrangement then resolves the new topology: it splits
edges at the new crossings, orders the edges at every vertex again, and
the faces take their values from the 1x pixels they contain (section 4),
so each face keeps its colour.

- **Fixed point, no float in topology.** The arrangement takes a scale
  `K` (a power of two): coordinates are picture coordinates times `2K`.
  `K = 1` is the doubled grid of section 2 and is used for every plane
  without such a shape, so those are built exactly as before. With a
  curved shape the plane is built at `K = 8` (1/16 original pixel, finer
  than the 0.25 px chords; at N = 10 one unit is 0.625 HD pixel, within
  one HD pixel of the 16 x 16 sample grid). Every existing segment is
  scaled exactly (integers times K), so all its vertices are K times the
  old ones and, K being a power of two, convert to the same picture
  coordinates bit for bit: faces away from the shape, the 3b fits and the
  shared boundary of other shapes come out as before. Ranges at K = 8:
  coordinates below 2^14, directions below 2^15, cross products below
  2^31, intersection numerators below 2^47; comparing two rational points
  needs 2^78, so point comparison and the orientation of a rational
  point use 128-bit products (`sci/core/exact.hpp`); everything else
  stays in checked `int64_t`.
- **The shape's segments.** Its lines' segments are replaced by the
  chords of its pieces, each chord end rounded to the K grid (the path's
  ends are pixel centres and stay exact; the end shared by two lines is
  one point). Where the gap closer extended a line of the shape at a path
  end, the extension stays, from the pixel centre along the line. The
  connectors between the shape's own inner ends go. Every other segment
  or connector that ended on one of the shape's old segments (a T joint,
  an extension, a join to an inner end) is moved along its own line to
  the nearest crossing with the chords within 2 original pixels (exact,
  `line_meets_segment`); where it reaches over the curve the crossing
  splits it and the part beyond is a dangling edge with one face on both
  sides. Without such a crossing the shape is not entered.
- **Acceptance.** As for gap hints and straight shapes: the shape is kept
  only if the partition gets no more mixed faces and 1x-block pixels.
  Thin slivers between the chords and nearby line ends lie under the
  shape's stroke. The round trip, the 6.4 line rule (with the customer's
  1a exception), the 6.4 acceptance and A1 are measured on the result as
  before. A shape that is not entered keeps its steered stroke and 3b
  faces, reported with the reason ("the faces keep the 3b fit: ...").
- **Strokes** stay the unrounded pieces (at most 1/32 px from the chords,
  under the stroke).

As built (`with_curve`, `enter_curved_shapes` in `build_scene.cpp`):

- **Which shapes.** A steered shape that passes the round trip and the
  line check, whose faces the shared boundary cannot take, and that is
  not a `line` with pixel-centre ends (those enter as one segment). The
  shapes of a plane enter jointly if the partition takes them so, else
  one by one. A shape entered can make a neighbour that shared by moving
  vertices fail to (an arch's lit band beside its inner arch): the plane
  is built again with the shapes entered and the steering repeated, at
  most three rounds; straight shapes enter in the first round only.
- **The connectors at the shape's inner ends** that join its own lines
  or extend one of them along its line go. A connector from another
  line's end to an inner end, and every other edge ending on the shape's
  old segments, moves along its own line to the nearest crossing with the
  chords within 2 px. Where its line does not reach the curve (it runs
  along it): an end on the grid stays and a connector joins it to the
  chords' nearest point on the grid; a line the gap closer extended onto
  the shape goes back to its own end pixel and is joined so. Otherwise
  the shape is not entered ("the line ending on the shape at (x, y) does
  not reach its curve within 2 px").
- **Chain hints (3a) on lines of a shape entered** are superseded: the
  steered shape is their boundary ("its lines are a steered shape, a
  curve of the partition").
- **Report.** "shared by the faces (a curve of the partition)"; a shape
  not entered keeps "the faces keep the 3b fit: ..." with "; not a curve
  of the partition: <reason>" (for the mixed-face rule, the new mixed
  faces' boxes).
- **Cost.** Each round builds the plane again; room entry of the pictures
  with such shapes took 2 to 4 times as long at N = 1. The fast analysis
  below brings it back to about the pre-curve times (`docs/hd-quality.md`,
  "Room entry after the speed-up").

### Room entry: the fast analysis and the reference (milestone 10)

`build_scene` takes an `Analysis`: `Fast` (the default, used everywhere)
or `Reference`, the simple implementation it is checked against (spec:
optimisations only after measurement, the reference kept for differential
tests). Both give the same scene and the same report, bit for bit; the
fast analysis only finds the same things sooner. Measured first
(`sci-hd-report`, N = 1, a profile per step): most of the time went to
building arrangements (several per round, each testing every pair of
segments whose boxes overlap) and to the shared boundary
(`SharedBoundary::apply` scanned every vertex and half-edge for each
vertex it moved, and the box of every edge for each edge it moved), not
to the rounds as such.

| Step | Reference | Fast analysis |
|---|---|---|
| Pairs of segments to intersect | every pair, box test | a grid of cells 16 px square; a pair is met in the cell of its boxes' overlap's top left corner only |
| Partitions | each one built | the last four kept per plane; the same segments at the same scale again (the start of the round after a shape entered, the straight shapes' base) give a copy |
| The doubled-grid partition a curved shape is checked against | built again on the fine grid | the doubled-grid one already made (segments scaled exactly give the same arrangement, section 9 and its unit test) |
| Shared boundary: half-edges at a vertex, edges of a line, vertices near a line end | scan of all | indexes made once per arrangement (shared by the copies the relation solver makes) |
| Shared boundary: boxes of the edges not moved | computed per test | kept per edge, updated when a shape is applied |

Two exact shortcuts serve both: products of factors below 2^31 compare in
64 bits (`sci::core::compare_products`, `wide_mul` otherwise), and two
segments that run from their `a` to their `b` and cross outside one of
them are passed over before the crossing point is made (its fractions
along both are compared with the denominator); a crossing at a segment's
end is that end. The rounds, the joint-then-one-by-one entry and the fine
grid are unchanged: no shape enters differently.

**Differential tests.** `sci-golden-hashes <game> <file> --reference`
builds both planes of every picture with both analyses and compares the
scenes (`write_scene`) and the hints' outcomes; the unit test
`a_steered_oval_builds_the_same_scene_in_both_analyses` does so on a
synthetic decided ellipse. The golden hashes of every entry are those of
the build before.

### Relation solving (milestone 10, A4 A.3)

`solve_object` in `build_scene.cpp` with `src/sci-vector/src/relations.*`,
run for each object before its primitives are steered:

- **Which objects.** Those that steer (decided or certain, not organic,
  not `keep`) and whose followed hypothesis has relations. An object
  whose **main hypothesis** the customer chose over alternatives keeps its
  relations unsolved: the main hypothesis is the "as drawn" reading
  (picture 12's tall window, "keep as drawn": its bars are not touched;
  picture 74's stairs, "drawn heights"). An **alternative** the customer
  chose has its relations solved (`keeps_relations_as_drawn`,
  `sci/vector/hints.hpp`). Open and organic objects are never solved.
- **Unknowns.** The anchor points of the object's `line` (its path's two
  ends) and `polyline` (its vertices) primitives that are items of a
  relation (`anchors` in `scene_model.hpp`). Points the hint gives equal
  in two primitives are one point (a shared corner, tied). Arcs, ellipses
  and splines are not unknowns; a relation with such an item is not
  solved and is listed ("an item is not a steered line or polyline").
- **Relations.** `symmetry` (axis horizontal or vertical): the items'
  points (tied ones as one) are paired with the point nearest to their
  mirror image about the points' mean, the pairing must be mutual (a point
  paired with itself lies on the axis); paired points share the other
  coordinate and every pair's midpoint lies on one axis, whose position is
  part of the solution. `parallel`, `perpendicular`: of line items (a part
  stands for its lines), cross or dot product of their directions zero.
  `aligned` (axis): the items' centres (mean of their anchors) share one
  x (vertical) or y (horizontal). `equal-spacing`: consecutive centres are
  equally far apart across the first item's first line, as drawn.
  `tangent`: not solved yet (listed).
- **Solution.** Least squares on the points' displacements subject to the
  relations: x = x0 + J^T lambda with (J J^T) lambda = -g(x) - J (x0 - x),
  repeated (Gauss-Newton; one step for linear relations, at most 50),
  dependent rows (two symmetries state some facts twice) left out by
  Gram-Schmidt, Gaussian elimination with partial pivoting; +, -, *, /
  and sqrt only, deterministic. Afterwards every relation must hold
  (1e-9), and no point may move by more than 1 original pixel
  (Chebyshev, A.6); otherwise nothing is solved and the report says why
  ("the relations need a move of ... px").
- **Then the usual steering.** A solved line or polyline runs through its
  solved points (a polyline's lines meet at its solved vertices), and
  passes the round trip, the line check (with the approved deviation for
  shapes the customer decided) and the shared boundary as before. If any
  solved shape fails the round trip or the line check, none is used and
  the object is steered unsolved ("the solved relations fail ..."); a
  primitive that fails that too keeps its 3b reconstruction. For an
  object the customer did not decide, solving must also not cost the
  shared boundary: if more of its solved shapes than unsolved ones keep
  the 3b faces, the unsolved shapes are steered ("the solved relations
  cost the shared boundary"). Ends of two shapes of a solved object that
  meet at one solved point are fixed for the stroke integrity pass, like
  the inner ends of one shape.
- **Report.** "relations solved jointly (...), largest move ... px" among
  the hints used; every relation not solved, with the reason, among those
  left out. Relations that hold as drawn change nothing and are not
  listed.

## 7. Rasterisation

`sci-render` samples a scene on a 16 x 16 grid per HD pixel (spec section
6.6). Sample i of a row of density d lies at x = (i + 0.5) / d.

- **Faces**: the scene's scan edges (every non-horizontal boundary, with the
  face on its +x side) are crossed with the sample row; a sample takes the
  face of the last crossing at or before it. Of crossings at the same x,
  the edge closest to +x comes last. Every sample therefore lies in exactly
  one face.
- **Layers** are painted over the faces in order, as spans: the capsule of
  each stroke segment, the ellipse, the box, the blocks.
- **Colour**: a pixel is the average of its 256 samples. Each pair's colour
  is the plain average of its two EGA colours' sRGB values (spec section
  6.5) at 16 bits per channel (v8 * 257); sums are integers, and the 8-bit
  result is round(v16 * 255 / 65535) by `((v16 * 255) + 32768) >> 16`.
  The table is built at compile time (`sci/render/colors.hpp`).
- **Fast paths**: face spans are filled in bulk, and a pixel row of 16 equal
  samples is added in one step. `reference_value` evaluates one point the
  simple way; a differential test compares the two.
- **Threads**: HD rows are tasks of the executor interface; each writes its
  own row, so the result does not depend on the threads.
- **HD priority map**: the priority scene sampled once per HD pixel, at its
  centre, so occlusion edges follow the vectors (spec section 6.6).
- **Original mode**: the 1x visual map, every pixel an N x N block.

## 8. Acceptance

`check_acceptance` (`sci/render/acceptance.hpp`) reduces the visual scene to
1x (per pixel the value covering most of its 16 x 16 samples) and compares
pair classes (pairs compared by the colour they blend to) with path 1's
pair map outside the edge band: the agreement must be at least 99% and no
group of four or more differing pixels may remain (a swap). Every pixel a
visible line owns at 1x must be touched by its stroke.

### The A1 checks

`check_integrity` (`sci/render/integrity.hpp`) samples the visual scene at
8 x 8 points per original pixel, recording the layer that shows at each,
and compares with path 1:

- **Overshoot**: at a line end on a contour (as in step 2 above), samples
  of the line inside the contour's stroke or beyond its line, from 2
  samples on.
- **Depth**: a layer showing outside its own 1x pixels and the edge band
  where the 1x priority is nearer than its own.
- **Doubled boundary**: two lines that drew two or more pixels in common;
  on those pixels HD may not show one line where the player saw the other
  (a quarter of a pixel or more).
- **Fragment**: every 4-connected group of samples showing a line contains
  a sample in a pixel the line owns at 1x.
- **Break**: the samples over one 8-connected group of a line's owned
  pixels are connected, and at every joint (a seen end pixel touching
  another line's seen pixel) the line reaches the other line's stroke
  shape.

Residual cases carry a size and, for overshoots and joints, a class used by
the milestone 3b report: the end pixel lies past the contour at 1x, the
line meets the contour at under 30 degrees, or the end meets another line
it touches.

### The round-trip check (A4 A.6, milestone 10)

`check_round_trip` (`sci/render/round_trip.hpp`) takes every fitted chain
(`SceneStats::fitted_chains`) and the strokes of its lines as fitted,
before the stroke integrity pass (`SceneStats::fitted_strokes`),
rasterises them back to 1x (points snapped to their pixels, staircase
corners dropped, joined by path 1's line rule) and compares them with the
pixels the lines drew: Chebyshev deviation (maximum and mean), pixels
added and missed. A chain passes at a deviation of at most 1 original
pixel. The baseline is in `docs/hd-quality.md`.

## 9. Determinism

Topology is exact, on the doubled grid or, for a plane with a curved
shape entered, the grid of 1/16 pixel (comparisons of points with 128-bit
products, `sci::core::wide_mul`, no compiler extension; the curve's chord
ends rounded half up). Fitting and rasterisation use `double` under the
floating-point profile of spec section 5.3; orders that reach the output
use stable sorts with explicit tie-breakers. Golden hashes of synthetic
scenes run in CI on x86-64 and ARM64, and those of every picture of the
tested release are checked locally (`tools/golden-hashes`), also on ARM64
under qemu-user.
