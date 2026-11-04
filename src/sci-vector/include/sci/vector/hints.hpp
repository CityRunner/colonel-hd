// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scene-analysis hints (spec section 6.7): per picture, which chains of PIC
// lines form one curve and of what kind, which gaps the artist meant to
// close, labels for regions, and what is in front of or contains what. The
// analysis is done offline; its results are committed as
// hints/pic.NNN.json and frozen at the milestone 3b gate
// (docs/SPEC_ADDENDA.md, A1).
// The format is described in hints/README.md and hints/schema.json.
//
// Hints are untrusted input and only advice: the vector analysis applies a
// hint only where it keeps the acceptance criteria (spec section 6.4) and
// otherwise falls back to the automatic result. Hints refer to lines by
// their PIC command, never by coordinates, so they hold for mirrored
// drawings too.
#pragma once

#include "sci/core/error.hpp"
#include "sci/core/sha256.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sci::vector {

// A line of the picture: the index of its command in the picture's command
// stream as sci-pic parses it (one line per command).
using LineRef = std::uint32_t;

enum class HintPrimitive : std::int8_t { Line, Arc, Spline, Ellipse };

// Lines that together imitate one curve of the given kind.
struct ChainHint {
    std::string id;
    std::vector<LineRef> lines;
    HintPrimitive primitive = HintPrimitive::Spline;
    std::string label;
    bool uncertain = false;
};

enum class LineEnd : std::int8_t { Start, End };

// A gap the artist meant to close: from one end of a line to the nearest
// point of another line.
struct GapHint {
    std::string id;
    LineRef from = 0;
    LineEnd end = LineEnd::Start;
    LineRef to = 0;
    std::string label;
    bool uncertain = false;
};

// What a region shows; informational. `x`, `y` is a pixel of the region in
// picture coordinates as stored (before mirroring).
struct RegionHint {
    std::string id;
    int x = 0;
    int y = 0;
    std::string label;
    bool uncertain = false;
};

enum class SymmetryKind : std::int8_t { Mirror, Repeat };

// Chains that mirror or repeat each other; informational.
struct SymmetryHint {
    SymmetryKind kind = SymmetryKind::Mirror;
    std::vector<std::string> chains; // Chain ids.
    std::string note;
};

enum class RelationKind : std::int8_t { InFrontOf, Behind, Contains };

// Depth or containment between two regions or chains, by id: `a` is in
// front of, behind, or contains `b` (spec section 6.10, addendum A1). Used
// by the vector path from milestone 3b.
struct RelationHint {
    RelationKind kind = RelationKind::InFrontOf;
    std::string a;
    std::string b;
    std::string note;
    bool uncertain = false;
};

// The scene model (format version 2; docs/SPEC_ADDENDA.md A4, A.1 to A.7):
// what the picture shows, as objects made of parts made of primitives,
// anchored to PIC command indices and 1x points. Information for milestone
// 10; the vector path does not use it yet.

// A.2. The organic types (see is_organic) get no geometric idealisation.
enum class ObjectType : std::int8_t {
    Window,
    Door,
    Arch,
    Staircase,
    Column,
    Roof,
    Railing,
    Fence,
    Gate,
    Furniture,
    Fireplace,
    Curtain,
    Painting,
    Wall,
    Floor,
    Ceiling,
    Building,
    Bridge,
    Dock,
    Well,
    Tombstone,
    Path,
    Foliage,
    Tree,
    Rock,
    Cloud,
    Water,
    Terrain,
    Figure,
    Other,
};

// Trees, foliage, rocks, clouds, water, terrain and figures (A.4): only
// splines and polylines, no geometric relations, no repetitions.
[[nodiscard]] bool is_organic(ObjectType type) noexcept;

// What a part is to its object.
enum class PartRole : std::int8_t {
    Outline,
    Edge,
    Frame,
    GlazingBar,
    Pane,
    Sill,
    Lintel,
    Jamb,
    Leaf,
    Panel,
    Handle,
    Hinge,
    Step,
    Riser,
    Tread,
    Landing,
    Baluster,
    Handrail,
    Newel,
    Post,
    Rail,
    Picket,
    Shaft,
    Capital,
    Base,
    Ridge,
    Eave,
    Gable,
    Brick,
    Crenellation,
    Board,
    Leg,
    Top,
    Shelf,
    Drawer,
    Rod,
    Valance,
    Fold,
    Trunk,
    Branch,
    Crown,
    Root,
    Contour,
    Ripple,
    Shadow,
    Highlight,
    Opening,
    Other,
};

// The intended shape; a polyline has sharp corners at its inner points.
enum class PrimitiveKind : std::int8_t { Line, Arc, Ellipse, Spline, Polyline };

// A corner meant by the artist: sharp, rounded, or an intentional step
// (A.4: kept, while raster staircasing is smoothed).
enum class CornerKind : std::int8_t { Sharp, Rounded, Step };

// A 1x pixel in picture coordinates as stored (before mirroring).
struct Point1x {
    int x = 0;
    int y = 0;

    friend bool operator==(const Point1x&, const Point1x&) = default;
};

struct CornerHint {
    std::size_t point = 0; // Index into the primitive's points.
    CornerKind kind = CornerKind::Sharp;
};

// What some commands draw. The points are its ends, corners and points it
// passes through, in order: a line has 2, an arc 3 (start, a point on it,
// end), an ellipse at least 3 on it, a spline or polyline at least 2.
struct PrimitiveHint {
    std::string id;
    PrimitiveKind kind = PrimitiveKind::Line;
    std::vector<LineRef> commands;
    std::vector<Point1x> points;
    std::vector<CornerHint> corners;
};

struct PartHint {
    std::string id;
    PartRole role = PartRole::Other;
    std::string label;
    std::vector<PrimitiveHint> primitives;
};

// A.3. Symmetry and alignment have an axis; perpendicular and tangent join
// two items (tangent: an arc and a line primitive); equal spacing needs
// three or more.
enum class GeometricKind : std::int8_t { Parallel, Perpendicular, EqualSpacing, Symmetry, Tangent, Aligned };
enum class Axis : std::int8_t { None, Horizontal, Vertical };

// Between parts or primitives of one hypothesis, by id.
struct GeometricRelation {
    GeometricKind kind = GeometricKind::Parallel;
    std::vector<std::string> items;
    Axis axis = Axis::None;
    std::string note;
};

// One reading of an object (A.6): its type, parts and relations, with a
// confidence in per mille. The main hypothesis has the object's id.
struct Hypothesis {
    std::string id;
    ObjectType type = ObjectType::Other;
    int confidence = 0; // 0 to 1000.
    std::string note;
    std::vector<PartHint> parts;
    std::vector<GeometricRelation> relations;
};

// A.7: the customer's choice, with the cell reference of the answer. Once
// present, the object is never asked again.
struct Decision {
    std::string hypothesis; // The id of one of the object's hypotheses.
    std::string date;       // "YYYY-MM-DD".
    std::string cell;       // "B3", "B3.2" or "A2-A4" (A4 part B).
    std::string note;
};

struct ObjectHint {
    std::string id;
    std::string cell; // Where it is on the review grid, as for Decision.
    std::string label;
    Hypothesis main; // main.id == id.
    std::vector<Hypothesis> alternatives;
    std::optional<Decision> decision;
};

// A.5: objects of one type, or parts of one role, reconstructed identically.
// The analyser may be unsure (A.6, A.7): then the repetition has a
// confidence below 1000 and is put to the customer. A repetition has two
// fixed hypotheses: "repeat" (the items are reconstructed identically) and
// "as-drawn" (each item as its pixels show it, no repetition).
inline constexpr std::string_view repetition_repeat = "repeat";
inline constexpr std::string_view repetition_as_drawn = "as-drawn";

struct RepetitionHint {
    std::string id;
    std::vector<std::string> items;
    std::string cell;      // Where the items are on the review grid, as for Decision; may be empty.
    int confidence = 1000; // That the items are identical, 0 to 1000; certain when the file gives none.
    std::string note;
    // The customer's choice: hypothesis repetition_repeat or
    // repetition_as_drawn. Allowed on a certain repetition too, where it
    // overrides the analyser.
    std::optional<Decision> decision;
};

// Whether the repetition is still a question for the customer: uncertain
// (confidence below 1000) and not decided.
[[nodiscard]] bool is_open(const RepetitionHint& repetition) noexcept;

// What the renderer is to do (A.7): follow the decision if there is one;
// otherwise repeat a certain repetition, and draw an open one as drawn, so
// that the milestone 3b reconstruction stays until the customer answers.
[[nodiscard]] bool reconstructs_identically(const RepetitionHint& repetition) noexcept;

struct PictureHints {
    int version = 0;                 // As the file says; the members of version 1 mean the same in 2.
    int picture = 0;                 // PIC resource number.
    core::Sha256Digest pic_sha256{}; // Of the PIC resource data the hints were made for.
    std::string analyser;            // Who or what made the hints.
    std::vector<RegionHint> regions;
    std::vector<ChainHint> chains;
    std::vector<GapHint> gaps;
    std::vector<SymmetryHint> symmetry;
    std::vector<RelationHint> relations;
    std::vector<ObjectHint> objects;         // Version 2.
    std::vector<RepetitionHint> repetitions; // Version 2.
};

// The current version. Version 1 files are read as well: a version 1 file
// is a version 2 file without objects and repetitions (which version 1
// does not allow), and is migrated by changing its version to 2 when it
// first gains objects.
inline constexpr int hints_version = 2;
inline constexpr int hints_oldest_version = 1;

// Parses and validates a hint file. Every member is checked; unknown
// members are an error, so that typos do not pass silently.
[[nodiscard]] core::Result<PictureHints> parse_hints(std::string_view text);

// Whether the hints were made for this PIC resource (its decompressed data).
[[nodiscard]] bool hints_match(const PictureHints& hints, std::span<const std::uint8_t> pic_data);

// The hint file name of a picture: "pic.NNN.json".
[[nodiscard]] std::string hint_file_name(int picture);

} // namespace sci::vector
