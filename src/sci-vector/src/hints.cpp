// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/vector/hints.hpp"

#include "sci/core/json.hpp"
#include "sci/vector/scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>

namespace sci::vector {

namespace {

using core::JsonArray;
using core::JsonValue;

// Limits that keep a damaged or hostile file small to process.
constexpr std::size_t max_items = 4096;      // Per array of the file.
constexpr std::size_t max_chain_lines = 512; // Lines per chain.
constexpr std::size_t max_label = 200;       // Characters per label or note.
constexpr std::size_t max_id = 32;
constexpr std::int64_t max_command = 1'000'000;
constexpr std::int64_t max_picture = 999;

constexpr std::array primitive_names{
    std::pair{std::string_view("line"), HintPrimitive::Line},
    std::pair{std::string_view("arc"), HintPrimitive::Arc},
    std::pair{std::string_view("spline"), HintPrimitive::Spline},
    std::pair{std::string_view("ellipse"), HintPrimitive::Ellipse},
};

constexpr std::array end_names{
    std::pair{std::string_view("start"), LineEnd::Start},
    std::pair{std::string_view("end"), LineEnd::End},
};

constexpr std::array relation_names{
    std::pair{std::string_view("in-front-of"), RelationKind::InFrontOf},
    std::pair{std::string_view("behind"), RelationKind::Behind},
    std::pair{std::string_view("contains"), RelationKind::Contains},
};

constexpr std::array symmetry_names{
    std::pair{std::string_view("mirror"), SymmetryKind::Mirror},
    std::pair{std::string_view("repeat"), SymmetryKind::Repeat},
};

// The scene model (version 2).
constexpr std::size_t max_alternatives = 8; // Per object.
constexpr std::size_t max_list = 512;       // Parts, primitives, commands, points, corners, items.
constexpr std::int64_t max_confidence = 1000;

constexpr std::array object_type_names{
    std::pair{std::string_view("window"), ObjectType::Window},
    std::pair{std::string_view("door"), ObjectType::Door},
    std::pair{std::string_view("arch"), ObjectType::Arch},
    std::pair{std::string_view("staircase"), ObjectType::Staircase},
    std::pair{std::string_view("column"), ObjectType::Column},
    std::pair{std::string_view("roof"), ObjectType::Roof},
    std::pair{std::string_view("railing"), ObjectType::Railing},
    std::pair{std::string_view("fence"), ObjectType::Fence},
    std::pair{std::string_view("gate"), ObjectType::Gate},
    std::pair{std::string_view("furniture"), ObjectType::Furniture},
    std::pair{std::string_view("fireplace"), ObjectType::Fireplace},
    std::pair{std::string_view("curtain"), ObjectType::Curtain},
    std::pair{std::string_view("painting"), ObjectType::Painting},
    std::pair{std::string_view("wall"), ObjectType::Wall},
    std::pair{std::string_view("floor"), ObjectType::Floor},
    std::pair{std::string_view("ceiling"), ObjectType::Ceiling},
    std::pair{std::string_view("building"), ObjectType::Building},
    std::pair{std::string_view("bridge"), ObjectType::Bridge},
    std::pair{std::string_view("dock"), ObjectType::Dock},
    std::pair{std::string_view("well"), ObjectType::Well},
    std::pair{std::string_view("tombstone"), ObjectType::Tombstone},
    std::pair{std::string_view("path"), ObjectType::Path},
    std::pair{std::string_view("foliage"), ObjectType::Foliage},
    std::pair{std::string_view("tree"), ObjectType::Tree},
    std::pair{std::string_view("rock"), ObjectType::Rock},
    std::pair{std::string_view("cloud"), ObjectType::Cloud},
    std::pair{std::string_view("water"), ObjectType::Water},
    std::pair{std::string_view("terrain"), ObjectType::Terrain},
    std::pair{std::string_view("figure"), ObjectType::Figure},
    std::pair{std::string_view("other"), ObjectType::Other},
};

constexpr std::array role_names{
    std::pair{std::string_view("outline"), PartRole::Outline},
    std::pair{std::string_view("edge"), PartRole::Edge},
    std::pair{std::string_view("frame"), PartRole::Frame},
    std::pair{std::string_view("glazing-bar"), PartRole::GlazingBar},
    std::pair{std::string_view("pane"), PartRole::Pane},
    std::pair{std::string_view("sill"), PartRole::Sill},
    std::pair{std::string_view("lintel"), PartRole::Lintel},
    std::pair{std::string_view("jamb"), PartRole::Jamb},
    std::pair{std::string_view("leaf"), PartRole::Leaf},
    std::pair{std::string_view("panel"), PartRole::Panel},
    std::pair{std::string_view("handle"), PartRole::Handle},
    std::pair{std::string_view("hinge"), PartRole::Hinge},
    std::pair{std::string_view("step"), PartRole::Step},
    std::pair{std::string_view("riser"), PartRole::Riser},
    std::pair{std::string_view("tread"), PartRole::Tread},
    std::pair{std::string_view("landing"), PartRole::Landing},
    std::pair{std::string_view("baluster"), PartRole::Baluster},
    std::pair{std::string_view("handrail"), PartRole::Handrail},
    std::pair{std::string_view("newel"), PartRole::Newel},
    std::pair{std::string_view("post"), PartRole::Post},
    std::pair{std::string_view("rail"), PartRole::Rail},
    std::pair{std::string_view("picket"), PartRole::Picket},
    std::pair{std::string_view("shaft"), PartRole::Shaft},
    std::pair{std::string_view("capital"), PartRole::Capital},
    std::pair{std::string_view("base"), PartRole::Base},
    std::pair{std::string_view("ridge"), PartRole::Ridge},
    std::pair{std::string_view("eave"), PartRole::Eave},
    std::pair{std::string_view("gable"), PartRole::Gable},
    std::pair{std::string_view("brick"), PartRole::Brick},
    std::pair{std::string_view("crenellation"), PartRole::Crenellation},
    std::pair{std::string_view("board"), PartRole::Board},
    std::pair{std::string_view("leg"), PartRole::Leg},
    std::pair{std::string_view("top"), PartRole::Top},
    std::pair{std::string_view("shelf"), PartRole::Shelf},
    std::pair{std::string_view("drawer"), PartRole::Drawer},
    std::pair{std::string_view("rod"), PartRole::Rod},
    std::pair{std::string_view("valance"), PartRole::Valance},
    std::pair{std::string_view("fold"), PartRole::Fold},
    std::pair{std::string_view("trunk"), PartRole::Trunk},
    std::pair{std::string_view("branch"), PartRole::Branch},
    std::pair{std::string_view("crown"), PartRole::Crown},
    std::pair{std::string_view("root"), PartRole::Root},
    std::pair{std::string_view("contour"), PartRole::Contour},
    std::pair{std::string_view("ripple"), PartRole::Ripple},
    std::pair{std::string_view("shadow"), PartRole::Shadow},
    std::pair{std::string_view("highlight"), PartRole::Highlight},
    std::pair{std::string_view("opening"), PartRole::Opening},
    std::pair{std::string_view("other"), PartRole::Other},
};

constexpr std::array primitive_kind_names{
    std::pair{std::string_view("line"), PrimitiveKind::Line},
    std::pair{std::string_view("arc"), PrimitiveKind::Arc},
    std::pair{std::string_view("ellipse"), PrimitiveKind::Ellipse},
    std::pair{std::string_view("spline"), PrimitiveKind::Spline},
    std::pair{std::string_view("polyline"), PrimitiveKind::Polyline},
};

constexpr std::array corner_names{
    std::pair{std::string_view("sharp"), CornerKind::Sharp},
    std::pair{std::string_view("rounded"), CornerKind::Rounded},
    std::pair{std::string_view("step"), CornerKind::Step},
};

constexpr std::array geometric_names{
    std::pair{std::string_view("parallel"), GeometricKind::Parallel},
    std::pair{std::string_view("perpendicular"), GeometricKind::Perpendicular},
    std::pair{std::string_view("equal-spacing"), GeometricKind::EqualSpacing},
    std::pair{std::string_view("symmetry"), GeometricKind::Symmetry},
    std::pair{std::string_view("tangent"), GeometricKind::Tangent},
    std::pair{std::string_view("aligned"), GeometricKind::Aligned},
};

constexpr std::array axis_names{
    std::pair{std::string_view("horizontal"), Axis::Horizontal},
    std::pair{std::string_view("vertical"), Axis::Vertical},
};

// Reads the values of a hint file. The first problem is recorded; after
// it, readers return defaults and the caller's result is discarded.
class Reader {
public:
    [[nodiscard]] bool ok() const { return !error_; }

    [[nodiscard]] std::string error() const { return error_.value_or(""); }

    void fail(std::string_view where, std::string_view what) {
        if (!error_) {
            error_ = std::format("hints: {}: {}", where, what);
        }
    }

    // Checks that the value is an object with all required members and no
    // others than the optional ones.
    bool members(const JsonValue& value, std::string_view where, std::initializer_list<std::string_view> required,
                 std::initializer_list<std::string_view> optional = {}) {
        const core::JsonObject* object = value.object();
        if (object == nullptr) {
            fail(where, "object expected");
            return false;
        }
        for (const auto& [name, member] : *object) {
            if (std::ranges::find(required, name) == required.end() &&
                std::ranges::find(optional, name) == optional.end()) {
                fail(where, std::format("unknown member \"{}\"", name));
                return false;
            }
        }
        const auto* const missing =
            std::ranges::find_if(required, [&](std::string_view name) { return value.member(name) == nullptr; });
        if (missing != required.end()) {
            fail(where, std::format("member \"{}\" missing", *missing));
            return false;
        }
        return true;
    }

    std::int64_t integer(const JsonValue& value, std::string_view where, std::int64_t low, std::int64_t high) {
        const std::int64_t* number = value.integer();
        if (number == nullptr) {
            fail(where, "integer expected");
            return low;
        }
        if (*number < low || *number > high) {
            fail(where, std::format("{} not in {}..{}", *number, low, high));
            return low;
        }
        return *number;
    }

    // Printable ASCII of at most max_length characters; absent is empty.
    std::string text(const JsonValue* value, std::string_view where, std::size_t max_length = max_label) {
        if (value == nullptr) {
            return {};
        }
        const std::string* string = value->string();
        if (string == nullptr) {
            fail(where, "string expected");
            return {};
        }
        if (string->size() > max_length) {
            fail(where, "string too long");
            return {};
        }
        if (std::ranges::any_of(*string, [](char c) { return c < ' ' || c > '~'; })) {
            fail(where, "only printable ASCII is allowed");
            return {};
        }
        return *string;
    }

    std::string identifier(const JsonValue& value, std::string_view where) {
        std::string id = text(&value, where, max_id);
        const auto allowed = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; };
        if (ok() && (id.empty() || !std::ranges::all_of(id, allowed))) {
            fail(where, "ids are 1 to 32 characters of a-z, 0-9 and -");
        }
        return id;
    }

    // Absent is false.
    bool flag(const JsonValue* value, std::string_view where) {
        if (value == nullptr) {
            return false;
        }
        const bool* b = value->boolean();
        if (b == nullptr) {
            fail(where, "true or false expected");
            return false;
        }
        return *b;
    }

    // An array of min_size to max_size items; absent is empty.
    const JsonArray& array(const JsonValue* value, std::string_view where, std::size_t min_size, std::size_t max_size) {
        static const JsonArray empty;
        const JsonArray* items = value == nullptr ? &empty : value->array();
        if (items == nullptr) {
            fail(where, "array expected");
            return empty;
        }
        if (items->size() < min_size || items->size() > max_size) {
            fail(where, std::format("{} to {} items expected", min_size, max_size));
            return empty;
        }
        return *items;
    }

    template <typename Enum, std::size_t N>
    Enum choice(const JsonValue& value, std::string_view where,
                const std::array<std::pair<std::string_view, Enum>, N>& names) {
        const std::string* string = value.string();
        if (string != nullptr) {
            for (const auto& [name, e] : names) {
                if (*string == name) {
                    return e;
                }
            }
        }
        std::string expected;
        for (const auto& [name, e] : names) {
            expected += std::format("{}\"{}\"", expected.empty() ? "" : ", ", name);
        }
        fail(where, std::format("one of {} expected", expected));
        return names[0].second;
    }

    // A reference to the review grid (A4 part B): "B3", "B3.2", or a range
    // of two of them, "A2-A4". Row letters are upper case here.
    std::string cell(const JsonValue& value, std::string_view where) {
        std::string text = this->text(&value, where, max_id);
        if (!ok()) {
            return text;
        }
        // One end at `at`; returns the position after it, or npos.
        const auto end_of = [&](std::size_t at) -> std::size_t {
            if (at + 2 > text.size() || text[at] < 'A' || text[at] > 'C' || text[at + 1] < '1' || text[at + 1] > '5') {
                return std::string::npos;
            }
            at += 2;
            if (at < text.size() && text[at] == '.') {
                if (at + 2 > text.size() || text[at + 1] < '1' || text[at + 1] > '4') {
                    return std::string::npos;
                }
                at += 2;
            }
            return at;
        };
        std::size_t at = end_of(0);
        if (at != std::string::npos && at < text.size()) {
            at = text[at] == '-' ? end_of(at + 1) : std::string::npos;
        }
        if (at != text.size()) {
            fail(where, "a cell (B3), quarter (B3.2) or range (A2-A4) of the review grid expected");
        }
        return text;
    }

    // A date "YYYY-MM-DD" of the Gregorian calendar.
    std::string date(const JsonValue& value, std::string_view where) {
        std::string text = this->text(&value, where, max_id);
        if (!ok()) {
            return text;
        }
        const auto digits = [&](std::size_t at, std::size_t count) {
            int number = 0;
            for (std::size_t i = at; i < at + count; ++i) {
                if (text[i] < '0' || text[i] > '9') {
                    return -1;
                }
                number = (number * 10) + (text[i] - '0');
            }
            return number;
        };
        const bool shaped = text.size() == 10 && text[4] == '-' && text[7] == '-';
        const int year = shaped ? digits(0, 4) : -1;
        const int month = shaped ? digits(5, 2) : -1;
        const int day = shaped ? digits(8, 2) : -1;
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        constexpr std::array<int, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (year < 0 || month < 1 || month > 12 || day < 1 ||
            day > days[static_cast<std::size_t>(month - 1)] + (month == 2 && leap ? 1 : 0)) {
            fail(where, "a date YYYY-MM-DD expected");
        }
        return text;
    }

    LineRef line_ref(const JsonValue& value, std::string_view where) {
        return static_cast<LineRef>(integer(value, where, 0, max_command));
    }

    core::Sha256Digest digest(const JsonValue& value, std::string_view where) {
        core::Sha256Digest digest{};
        const std::string* string = value.string();
        if (string == nullptr || string->size() != 2 * digest.size()) {
            fail(where, "64 lowercase hexadecimal digits expected");
            return digest;
        }
        for (std::size_t i = 0; i < string->size(); ++i) {
            const char c = (*string)[i];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a' + 10);
            } else {
                fail(where, "64 lowercase hexadecimal digits expected");
                return digest;
            }
            digest[i / 2] = static_cast<std::uint8_t>((static_cast<unsigned>(digest[i / 2]) << 4U) | digit);
        }
        return digest;
    }

private:
    std::optional<std::string> error_;
};

RegionHint read_region(Reader& in, const JsonValue& value, const std::string& where) {
    RegionHint region;
    if (!in.members(value, where, {"id", "at", "label"}, {"uncertain"})) {
        return region;
    }
    region.id = in.identifier(*value.member("id"), where + ".id");
    const JsonArray& at = in.array(value.member("at"), where + ".at", 2, 2);
    if (at.size() == 2) {
        region.x = static_cast<int>(in.integer(at[0], where + ".at", 0, picture_width - 1));
        region.y = static_cast<int>(in.integer(at[1], where + ".at", 0, picture_height - 1));
    }
    region.label = in.text(value.member("label"), where + ".label");
    region.uncertain = in.flag(value.member("uncertain"), where + ".uncertain");
    return region;
}

ChainHint read_chain(Reader& in, const JsonValue& value, const std::string& where) {
    ChainHint chain;
    if (!in.members(value, where, {"id", "lines", "primitive"}, {"label", "uncertain"})) {
        return chain;
    }
    chain.id = in.identifier(*value.member("id"), where + ".id");
    for (const JsonValue& item : in.array(value.member("lines"), where + ".lines", 2, max_chain_lines)) {
        const LineRef ref = in.line_ref(item, where + ".lines");
        if (in.ok() && std::ranges::find(chain.lines, ref) != chain.lines.end()) {
            in.fail(where + ".lines", "a line is listed twice");
        }
        chain.lines.push_back(ref);
    }
    chain.primitive = in.choice(*value.member("primitive"), where + ".primitive", primitive_names);
    chain.label = in.text(value.member("label"), where + ".label");
    chain.uncertain = in.flag(value.member("uncertain"), where + ".uncertain");
    return chain;
}

GapHint read_gap(Reader& in, const JsonValue& value, const std::string& where) {
    GapHint gap;
    if (!in.members(value, where, {"id", "from", "end", "to"}, {"label", "uncertain"})) {
        return gap;
    }
    gap.id = in.identifier(*value.member("id"), where + ".id");
    gap.from = in.line_ref(*value.member("from"), where + ".from");
    gap.end = in.choice(*value.member("end"), where + ".end", end_names);
    gap.to = in.line_ref(*value.member("to"), where + ".to");
    if (in.ok() && gap.from == gap.to) {
        in.fail(where, "a gap joins two different lines");
    }
    gap.label = in.text(value.member("label"), where + ".label");
    gap.uncertain = in.flag(value.member("uncertain"), where + ".uncertain");
    return gap;
}

SymmetryHint read_symmetry(Reader& in, const JsonValue& value, const std::string& where,
                           const std::vector<ChainHint>& chains) {
    SymmetryHint symmetry;
    if (!in.members(value, where, {"kind", "chains"}, {"note"})) {
        return symmetry;
    }
    symmetry.kind = in.choice(*value.member("kind"), where + ".kind", symmetry_names);
    for (const JsonValue& item : in.array(value.member("chains"), where + ".chains", 2, max_items)) {
        std::string id = in.identifier(item, where + ".chains");
        if (!in.ok()) {
            break;
        }
        if (std::ranges::none_of(chains, [&](const ChainHint& chain) { return chain.id == id; })) {
            in.fail(where + ".chains", std::format("no chain \"{}\"", id));
        } else if (std::ranges::find(symmetry.chains, id) != symmetry.chains.end()) {
            in.fail(where + ".chains", "a chain is listed twice");
        }
        symmetry.chains.push_back(std::move(id));
    }
    symmetry.note = in.text(value.member("note"), where + ".note");
    return symmetry;
}

// Whether a region or chain of the file has this id.
bool names_region_or_chain(const PictureHints& hints, const std::string& id) {
    return std::ranges::any_of(hints.regions, [&](const RegionHint& r) { return r.id == id; }) ||
           std::ranges::any_of(hints.chains, [&](const ChainHint& c) { return c.id == id; });
}

RelationHint read_relation(Reader& in, const JsonValue& value, const std::string& where, const PictureHints& hints) {
    RelationHint relation;
    if (!in.members(value, where, {"kind", "a", "b"}, {"note", "uncertain"})) {
        return relation;
    }
    relation.kind = in.choice(*value.member("kind"), where + ".kind", relation_names);
    relation.a = in.identifier(*value.member("a"), where + ".a");
    relation.b = in.identifier(*value.member("b"), where + ".b");
    if (in.ok()) {
        for (const std::string* id : {&relation.a, &relation.b}) {
            if (!names_region_or_chain(hints, *id)) {
                in.fail(where, std::format("no region or chain \"{}\"", *id));
                break;
            }
        }
    }
    if (in.ok() && relation.a == relation.b) {
        in.fail(where, "a relation joins two different regions or chains");
    }
    relation.note = in.text(value.member("note"), where + ".note");
    relation.uncertain = in.flag(value.member("uncertain"), where + ".uncertain");
    return relation;
}

template <typename T, typename Read>
std::vector<T> read_list(Reader& in, const JsonValue& file, std::string_view name, Read read) {
    std::vector<T> list;
    const JsonArray& items = in.array(file.member(name), name, 0, max_items);
    for (std::size_t i = 0; i < items.size() && in.ok(); ++i) {
        list.push_back(read(items[i], std::format("{}[{}]", name, i)));
    }
    return list;
}

// The scene model (version 2).

Point1x read_point(Reader& in, const JsonValue& value, const std::string& where) {
    const JsonArray& xy = in.array(&value, where, 2, 2);
    if (xy.size() != 2) {
        return {};
    }
    return {
        .x = static_cast<int>(in.integer(xy[0], where, 0, picture_width - 1)),
        .y = static_cast<int>(in.integer(xy[1], where, 0, picture_height - 1)),
    };
}

// The number of points each kind needs.
std::pair<std::size_t, std::size_t> point_range(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::Line:
        return {2, 2};
    case PrimitiveKind::Arc:
        return {3, 3};
    case PrimitiveKind::Ellipse:
        return {3, max_list};
    case PrimitiveKind::Spline:
    case PrimitiveKind::Polyline:
        break;
    }
    return {2, max_list};
}

PrimitiveHint read_primitive(Reader& in, const JsonValue& value, const std::string& where, bool organic) {
    PrimitiveHint primitive;
    if (!in.members(value, where, {"id", "kind", "commands", "points"}, {"corners"})) {
        return primitive;
    }
    primitive.id = in.identifier(*value.member("id"), where + ".id");
    primitive.kind = in.choice(*value.member("kind"), where + ".kind", primitive_kind_names);
    if (in.ok() && organic && primitive.kind != PrimitiveKind::Spline && primitive.kind != PrimitiveKind::Polyline) {
        in.fail(where + ".kind", "an organic object has only splines and polylines (A4 A.4)");
    }
    for (const JsonValue& item : in.array(value.member("commands"), where + ".commands", 1, max_list)) {
        const LineRef ref = in.line_ref(item, where + ".commands");
        if (in.ok() && std::ranges::find(primitive.commands, ref) != primitive.commands.end()) {
            in.fail(where + ".commands", "a command is listed twice");
        }
        primitive.commands.push_back(ref);
    }
    const auto [fewest, most] = point_range(primitive.kind);
    for (const JsonValue& item : in.array(value.member("points"), where + ".points", 2, max_list)) {
        primitive.points.push_back(read_point(in, item, where + ".points"));
    }
    if (in.ok() && (primitive.points.size() < fewest || primitive.points.size() > most)) {
        in.fail(where + ".points", std::format("{} to {} points expected for this kind", fewest, most));
    }
    for (const JsonValue& item : in.array(value.member("corners"), where + ".corners", 0, max_list)) {
        if (!in.members(item, where + ".corners", {"point", "kind"})) {
            break;
        }
        CornerHint corner;
        corner.point = static_cast<std::size_t>(in.integer(*item.member("point"), where + ".corners", 0,
                                                           static_cast<std::int64_t>(primitive.points.size()) - 1));
        corner.kind = in.choice(*item.member("kind"), where + ".corners", corner_names);
        if (in.ok() &&
            std::ranges::any_of(primitive.corners, [&](const CornerHint& c) { return c.point == corner.point; })) {
            in.fail(where + ".corners", "a point has two corners");
        }
        primitive.corners.push_back(corner);
    }
    return primitive;
}

PartHint read_part(Reader& in, const JsonValue& value, const std::string& where, bool organic) {
    PartHint part;
    if (!in.members(value, where, {"id", "role", "primitives"}, {"label"})) {
        return part;
    }
    part.id = in.identifier(*value.member("id"), where + ".id");
    part.role = in.choice(*value.member("role"), where + ".role", role_names);
    part.label = in.text(value.member("label"), where + ".label");
    const JsonArray& primitives = in.array(value.member("primitives"), where + ".primitives", 1, max_list);
    for (std::size_t i = 0; i < primitives.size() && in.ok(); ++i) {
        part.primitives.push_back(
            read_primitive(in, primitives[i], std::format("{}.primitives[{}]", where, i), organic));
    }
    return part;
}

const PrimitiveHint* find_primitive(const Hypothesis& hypothesis, const std::string& id) {
    for (const PartHint& part : hypothesis.parts) {
        for (const PrimitiveHint& primitive : part.primitives) {
            if (primitive.id == id) {
                return &primitive;
            }
        }
    }
    return nullptr;
}

GeometricRelation read_geometric(Reader& in, const JsonValue& value, const std::string& where,
                                 const Hypothesis& hypothesis) {
    GeometricRelation relation;
    if (!in.members(value, where, {"kind", "items"}, {"axis", "note"})) {
        return relation;
    }
    relation.kind = in.choice(*value.member("kind"), where + ".kind", geometric_names);
    // The kinds of the items that are primitives.
    std::vector<PrimitiveKind> primitives;
    for (const JsonValue& item : in.array(value.member("items"), where + ".items", 2, max_list)) {
        std::string id = in.identifier(item, where + ".items");
        if (!in.ok()) {
            break;
        }
        const PrimitiveHint* primitive = find_primitive(hypothesis, id);
        if (primitive != nullptr) {
            primitives.push_back(primitive->kind);
        } else if (std::ranges::none_of(hypothesis.parts, [&](const PartHint& p) { return p.id == id; })) {
            in.fail(where + ".items", std::format("no part or primitive \"{}\" in this hypothesis", id));
        }
        if (in.ok() && std::ranges::find(relation.items, id) != relation.items.end()) {
            in.fail(where + ".items", "an item is listed twice");
        }
        relation.items.push_back(std::move(id));
    }
    const auto count = [&](PrimitiveKind kind) { return std::ranges::count(primitives, kind); };
    if (const JsonValue* axis = value.member("axis"); axis != nullptr) {
        relation.axis = in.choice(*axis, where + ".axis", axis_names);
    }
    const bool needs_axis = relation.kind == GeometricKind::Symmetry || relation.kind == GeometricKind::Aligned;
    const bool pair = relation.kind == GeometricKind::Perpendicular || relation.kind == GeometricKind::Tangent;
    if (!in.ok()) {
        // Reported above.
    } else if (needs_axis != (relation.axis != Axis::None)) {
        in.fail(where, "symmetry and aligned have an axis, the other kinds none");
    } else if (pair && relation.items.size() != 2) {
        in.fail(where + ".items", "two items expected");
    } else if (relation.kind == GeometricKind::EqualSpacing && relation.items.size() < 3) {
        in.fail(where + ".items", "three or more items expected");
    } else if (relation.kind == GeometricKind::Tangent &&
               (primitives.size() != 2 || count(PrimitiveKind::Arc) + count(PrimitiveKind::Ellipse) != 1 ||
                count(PrimitiveKind::Line) != 1)) {
        in.fail(where + ".items", "a tangent joins an arc or ellipse and a line primitive");
    }
    relation.note = in.text(value.member("note"), where + ".note");
    return relation;
}

// The members of a hypothesis, of an object (the main one) or of one of
// its alternatives.
Hypothesis read_hypothesis(Reader& in, const JsonValue& value, const std::string& where) {
    Hypothesis hypothesis;
    hypothesis.id = in.identifier(*value.member("id"), where + ".id");
    hypothesis.type = in.choice(*value.member("type"), where + ".type", object_type_names);
    hypothesis.confidence =
        static_cast<int>(in.integer(*value.member("confidence"), where + ".confidence", 0, max_confidence));
    const bool organic = is_organic(hypothesis.type);
    const JsonArray& parts = in.array(value.member("parts"), where + ".parts", 1, max_list);
    for (std::size_t i = 0; i < parts.size() && in.ok(); ++i) {
        hypothesis.parts.push_back(read_part(in, parts[i], std::format("{}.parts[{}]", where, i), organic));
    }
    const JsonArray& relations = in.array(value.member("relations"), where + ".relations", 0, max_items);
    if (in.ok() && organic && !relations.empty()) {
        in.fail(where + ".relations", "an organic object has no geometric relations (A4 A.4)");
    }
    for (std::size_t i = 0; i < relations.size() && in.ok(); ++i) {
        hypothesis.relations.push_back(
            read_geometric(in, relations[i], std::format("{}.relations[{}]", where, i), hypothesis));
    }
    return hypothesis;
}

// A.7: the customer's choice, on an object or a repetition. The caller checks
// that it names a hypothesis of its item.
Decision read_decision(Reader& in, const JsonValue& value, const std::string& where) {
    Decision decision;
    if (!in.members(value, where, {"hypothesis", "by", "date", "cell"}, {"note"})) {
        return decision;
    }
    decision.hypothesis = in.identifier(*value.member("hypothesis"), where + ".hypothesis");
    if (in.text(value.member("by"), where + ".by") != "customer") {
        in.fail(where + ".by", "\"customer\" expected");
    }
    decision.date = in.date(*value.member("date"), where + ".date");
    decision.cell = in.cell(*value.member("cell"), where + ".cell");
    decision.note = in.text(value.member("note"), where + ".note");
    return decision;
}

ObjectHint read_object(Reader& in, const JsonValue& value, const std::string& where) {
    ObjectHint object;
    if (!in.members(value, where, {"id", "type", "cell", "confidence", "parts"},
                    {"label", "relations", "alternatives", "decision"})) {
        return object;
    }
    object.main = read_hypothesis(in, value, where);
    object.id = object.main.id;
    object.cell = in.cell(*value.member("cell"), where + ".cell");
    object.label = in.text(value.member("label"), where + ".label");
    const JsonArray& alternatives =
        in.array(value.member("alternatives"), where + ".alternatives", 0, max_alternatives);
    for (std::size_t i = 0; i < alternatives.size() && in.ok(); ++i) {
        const std::string at = std::format("{}.alternatives[{}]", where, i);
        if (!in.members(alternatives[i], at, {"id", "type", "confidence", "parts"}, {"note", "relations"})) {
            break;
        }
        Hypothesis alternative = read_hypothesis(in, alternatives[i], at);
        alternative.note = in.text(alternatives[i].member("note"), at + ".note");
        object.alternatives.push_back(std::move(alternative));
    }
    if (const JsonValue* decision = value.member("decision"); decision != nullptr && in.ok()) {
        const std::string at = where + ".decision";
        Decision d = read_decision(in, *decision, at);
        if (in.ok() && d.hypothesis != object.id &&
            std::ranges::none_of(object.alternatives, [&](const Hypothesis& h) { return h.id == d.hypothesis; })) {
            in.fail(at + ".hypothesis", std::format("no hypothesis \"{}\" of this object", d.hypothesis));
        }
        object.decision = std::move(d);
    }
    return object;
}

std::vector<const Hypothesis*> hypotheses_of(const ObjectHint& object) {
    std::vector<const Hypothesis*> all{&object.main};
    for (const Hypothesis& alternative : object.alternatives) {
        all.push_back(&alternative);
    }
    return all;
}

// A repetition's item: an object's type or a part's role, and whether it
// is organic.
struct RepeatedItem {
    bool object = false;
    int kind = 0;
    bool organic = false;
};

std::optional<RepeatedItem> find_repeated(const std::vector<ObjectHint>& objects, const std::string& id) {
    for (const ObjectHint& object : objects) {
        if (object.id == id) {
            return RepeatedItem{
                .object = true,
                .kind = static_cast<int>(object.main.type),
                .organic = is_organic(object.main.type),
            };
        }
        for (const Hypothesis* hypothesis : hypotheses_of(object)) {
            const auto part = std::ranges::find(hypothesis->parts, id, &PartHint::id);
            if (part != hypothesis->parts.end()) {
                return RepeatedItem{
                    .object = false,
                    .kind = static_cast<int>(part->role),
                    .organic = is_organic(hypothesis->type),
                };
            }
        }
    }
    return std::nullopt;
}

RepetitionHint read_repetition(Reader& in, const JsonValue& value, const std::string& where,
                               const std::vector<ObjectHint>& objects) {
    RepetitionHint repetition;
    if (!in.members(value, where, {"id", "items"}, {"cell", "confidence", "note", "decision"})) {
        return repetition;
    }
    repetition.id = in.identifier(*value.member("id"), where + ".id");
    std::vector<RepeatedItem> found;
    for (const JsonValue& item : in.array(value.member("items"), where + ".items", 2, max_list)) {
        std::string id = in.identifier(item, where + ".items");
        if (!in.ok()) {
            break;
        }
        if (const auto repeated = find_repeated(objects, id)) {
            found.push_back(*repeated);
        } else {
            in.fail(where + ".items", std::format("no object or part \"{}\"", id));
        }
        if (in.ok() && std::ranges::find(repetition.items, id) != repetition.items.end()) {
            in.fail(where + ".items", "an item is listed twice");
        }
        repetition.items.push_back(std::move(id));
    }
    const auto differs = [&](const RepeatedItem& f) { return f.object != found.front().object; };
    const auto other_kind = [&](const RepeatedItem& f) { return f.kind != found.front().kind; };
    if (!in.ok() || found.empty()) {
        // Reported above.
    } else if (std::ranges::any_of(found, differs)) {
        in.fail(where + ".items", "objects or parts expected, not both");
    } else if (std::ranges::any_of(found, other_kind)) {
        in.fail(where + ".items", "the items are of one type or of one role");
    } else if (std::ranges::any_of(found, &RepeatedItem::organic)) {
        in.fail(where + ".items", "organic shapes are not repeated identically (A4 A.4)");
    }
    if (const JsonValue* cell = value.member("cell"); cell != nullptr) {
        repetition.cell = in.cell(*cell, where + ".cell");
    }
    if (const JsonValue* confidence = value.member("confidence"); confidence != nullptr) {
        repetition.confidence = static_cast<int>(in.integer(*confidence, where + ".confidence", 0, max_confidence));
    }
    repetition.note = in.text(value.member("note"), where + ".note");
    if (const JsonValue* decision = value.member("decision"); decision != nullptr && in.ok()) {
        const std::string at = where + ".decision";
        Decision d = read_decision(in, *decision, at);
        if (in.ok() && d.hypothesis != repetition_repeat && d.hypothesis != repetition_as_drawn) {
            in.fail(at + ".hypothesis",
                    std::format("\"{}\" or \"{}\" expected", repetition_repeat, repetition_as_drawn));
        }
        repetition.decision = std::move(d);
    }
    return repetition;
}

// Ids are unique across the file, and no line is in two chains.
void check_unique(Reader& in, const PictureHints& hints) {
    std::vector<std::string> ids;
    ids.reserve(hints.regions.size() + hints.chains.size() + hints.gaps.size());
    for (const auto& region : hints.regions) {
        ids.push_back(region.id);
    }
    for (const auto& chain : hints.chains) {
        ids.push_back(chain.id);
    }
    for (const auto& gap : hints.gaps) {
        ids.push_back(gap.id);
    }
    for (const auto& object : hints.objects) {
        for (const Hypothesis* hypothesis : hypotheses_of(object)) {
            ids.push_back(hypothesis->id);
            for (const auto& part : hypothesis->parts) {
                ids.push_back(part.id);
                for (const auto& primitive : part.primitives) {
                    ids.push_back(primitive.id);
                }
            }
        }
    }
    std::vector<std::string> in_repetitions;
    for (const auto& repetition : hints.repetitions) {
        ids.push_back(repetition.id);
        in_repetitions.insert(in_repetitions.end(), repetition.items.begin(), repetition.items.end());
    }
    std::ranges::stable_sort(ids);
    if (const auto repeated = std::ranges::adjacent_find(ids); repeated != ids.end()) {
        in.fail("ids", std::format("\"{}\" is used twice", *repeated));
    }
    std::ranges::stable_sort(in_repetitions);
    if (const auto twice = std::ranges::adjacent_find(in_repetitions); twice != in_repetitions.end()) {
        in.fail("repetitions", std::format("\"{}\" is in two repetitions", *twice));
    }
    std::vector<LineRef> chained;
    for (const auto& chain : hints.chains) {
        chained.insert(chained.end(), chain.lines.begin(), chain.lines.end());
    }
    std::ranges::stable_sort(chained);
    if (const auto repeated = std::ranges::adjacent_find(chained); repeated != chained.end()) {
        in.fail("chains", std::format("line {} is in two chains", *repeated));
    }
    std::vector<std::tuple<RelationKind, std::string, std::string>> relations;
    relations.reserve(hints.relations.size());
    for (const auto& relation : hints.relations) {
        relations.emplace_back(relation.kind, relation.a, relation.b);
    }
    std::ranges::stable_sort(relations);
    if (std::ranges::adjacent_find(relations) != relations.end()) {
        in.fail("relations", "a relation is listed twice");
    }
}

} // namespace

core::Result<PictureHints> parse_hints(std::string_view text) {
    auto parsed = core::parse_json(text);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    const JsonValue& file = *parsed;
    Reader in;
    PictureHints hints;
    if (in.members(file, "file", {"format", "version", "picture", "pic_sha256", "analyser"},
                   {"regions", "chains", "gaps", "symmetry", "relations", "objects", "repetitions"})) {
        if (in.text(file.member("format"), "format") != "sierra-hd-hints") {
            in.fail("format", "\"sierra-hd-hints\" expected");
        }
        hints.version =
            static_cast<int>(in.integer(*file.member("version"), "version", hints_oldest_version, hints_version));
        if (in.ok() && hints.version < 2 &&
            (file.member("objects") != nullptr || file.member("repetitions") != nullptr)) {
            in.fail("version", "objects and repetitions need version 2");
        }
        hints.picture = static_cast<int>(in.integer(*file.member("picture"), "picture", 0, max_picture));
        hints.pic_sha256 = in.digest(*file.member("pic_sha256"), "pic_sha256");
        hints.analyser = in.text(file.member("analyser"), "analyser");
        const auto with = [&](auto read) {
            return [&in, read](const JsonValue& value, const std::string& where) { return read(in, value, where); };
        };
        hints.regions = read_list<RegionHint>(in, file, "regions", with(read_region));
        hints.chains = read_list<ChainHint>(in, file, "chains", with(read_chain));
        hints.gaps = read_list<GapHint>(in, file, "gaps", with(read_gap));
        hints.symmetry =
            read_list<SymmetryHint>(in, file, "symmetry", [&](const JsonValue& value, const std::string& where) {
                return read_symmetry(in, value, where, hints.chains);
            });
        hints.relations =
            read_list<RelationHint>(in, file, "relations", [&](const JsonValue& value, const std::string& where) {
                return read_relation(in, value, where, hints);
            });
        hints.objects = read_list<ObjectHint>(in, file, "objects", with(read_object));
        hints.repetitions =
            read_list<RepetitionHint>(in, file, "repetitions", [&](const JsonValue& value, const std::string& where) {
                return read_repetition(in, value, where, hints.objects);
            });
        check_unique(in, hints);
    }
    if (!in.ok()) {
        return core::make_error(core::ErrorCode::InvalidData, in.error());
    }
    return hints;
}

bool is_organic(ObjectType type) noexcept {
    constexpr std::array organic{
        ObjectType::Foliage, ObjectType::Tree,    ObjectType::Rock,   ObjectType::Cloud,
        ObjectType::Water,   ObjectType::Terrain, ObjectType::Figure,
    };
    return std::ranges::find(organic, type) != organic.end();
}

bool is_open(const RepetitionHint& repetition) noexcept {
    return !repetition.decision && repetition.confidence < max_confidence;
}

bool reconstructs_identically(const RepetitionHint& repetition) noexcept {
    if (repetition.decision) {
        return repetition.decision->hypothesis == repetition_repeat;
    }
    return !is_open(repetition);
}

bool hints_match(const PictureHints& hints, std::span<const std::uint8_t> pic_data) {
    return core::sha256(pic_data) == hints.pic_sha256;
}

std::string hint_file_name(int picture) {
    return std::format("pic.{:03}.json", picture);
}

} // namespace sci::vector
