// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/vector/hints.hpp"
#include "sci_test.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>

using sci::vector::HintPrimitive;
using sci::vector::LineEnd;
using sci::vector::parse_hints;

namespace {

// A valid file with one of everything; the tests below break it in turn.
const std::string valid = R"({
  "format": "sierra-hd-hints",
  "version": 1,
  "picture": 44,
  "pic_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "analyser": "test",
  "regions": [{"id": "sky", "at": [10, 5], "label": "night sky", "uncertain": true}],
  "chains": [
    {"id": "arch-left", "lines": [30, 31, 32], "primitive": "arc", "label": "arch"},
    {"id": "arch-right", "lines": [40, 41], "primitive": "spline"}
  ],
  "gaps": [{"id": "door-gap", "from": 72, "end": "end", "to": 90}],
  "symmetry": [{"kind": "mirror", "chains": ["arch-left", "arch-right"], "note": "both sides"}],
  "relations": [
    {"kind": "in-front-of", "a": "arch-left", "b": "sky"},
    {"kind": "contains", "a": "sky", "b": "arch-right", "uncertain": true, "note": "seen through"}
  ]
})";

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    SCI_CHECK(at != std::string::npos);
    if (at != std::string::npos) {
        text.replace(at, from.size(), to);
    }
    return text;
}

} // namespace

SCI_TEST(hints_read_a_complete_file) {
    const auto hints = parse_hints(valid);
    SCI_CHECK(hints.has_value());
    if (!hints) {
        return;
    }
    SCI_CHECK(hints->picture == 44);
    SCI_CHECK(hints->pic_sha256[0] == 0x01 && hints->pic_sha256[31] == 0xEF);
    SCI_CHECK(hints->regions.size() == 1 && hints->regions[0].x == 10 && hints->regions[0].uncertain);
    SCI_CHECK(hints->chains.size() == 2);
    SCI_CHECK(hints->chains[0].primitive == HintPrimitive::Arc);
    SCI_CHECK(hints->chains[0].lines[2] == 32);
    SCI_CHECK(!hints->chains[1].uncertain && hints->chains[1].label.empty());
    SCI_CHECK(hints->gaps.size() == 1 && hints->gaps[0].end == LineEnd::End);
    SCI_CHECK(hints->gaps[0].from == 72 && hints->gaps[0].to == 90);
    SCI_CHECK(hints->symmetry.size() == 1 && hints->symmetry[0].chains[1] == "arch-right");
    SCI_CHECK(hints->relations.size() == 2);
    SCI_CHECK(hints->relations[0].kind == sci::vector::RelationKind::InFrontOf && hints->relations[0].b == "sky");
    SCI_CHECK(hints->relations[1].kind == sci::vector::RelationKind::Contains && hints->relations[1].uncertain);
}

SCI_TEST(hints_arrays_are_optional) {
    const auto hints = parse_hints(R"({"format": "sierra-hd-hints", "version": 1, "picture": 0,
        "pic_sha256": "0000000000000000000000000000000000000000000000000000000000000000", "analyser": ""})");
    SCI_CHECK(hints.has_value() && hints->chains.empty() && hints->gaps.empty());
}

SCI_TEST(hints_reject_invalid_files) {
    const std::array<std::pair<const char*, const char*>, 22> breakages{
        {
            {R"("version": 1)", R"("version": 3)"},
            {R"("sierra-hd-hints")", R"("other")"},
            {R"("picture": 44)", R"("picture": 1000)"},
            {"0123456789abcdef\",", "0123456789abcdeF\","},
            {R"("analyser": "test")", R"("analyser": "t\u0001")"},
            {R"("at": [10, 5])", R"("at": [320, 5])"},
            {R"("at": [10, 5])", R"("at": [10])"},
            {R"("id": "sky")", R"("id": "Sky")"},
            {R"("id": "sky")", R"("id": "arch-left")"},
            {R"([30, 31, 32])", R"([30])"},
            {R"([30, 31, 32])", R"([30, 30])"},
            {R"([40, 41])", R"([40, 31])"},
            {R"("primitive": "arc")", R"("primitive": "circle")"},
            {R"("end": "end")", R"("end": "middle")"},
            {R"("to": 90)", R"("to": 72)"},
            {R"(["arch-left", "arch-right"])", R"(["arch-left", "nothing"])"},
            {R"("note": "both sides")", R"("notes": "both sides")"},
            {R"("uncertain": true)", R"("uncertain": 1)"},
            {R"("kind": "in-front-of")", R"("kind": "above")"},
            {R"("a": "arch-left", "b": "sky")", R"("a": "arch-left", "b": "moon")"},
            {R"("a": "arch-left", "b": "sky")", R"("a": "sky", "b": "sky")"},
            {
                R"("kind": "contains", "a": "sky", "b": "arch-right")",
                R"("kind": "in-front-of", "a": "arch-left", "b": "sky")",
            },
        },
    };
    for (const auto& [from, to] : breakages) {
        const auto hints = parse_hints(replaced(valid, from, to));
        SCI_CHECK(!hints.has_value());
    }
    SCI_CHECK(!parse_hints("[]").has_value());
    SCI_CHECK(!parse_hints("{").has_value());
}

SCI_TEST(hints_match_their_picture_data) {
    auto hints = parse_hints(valid);
    SCI_CHECK(hints.has_value());
    if (!hints) {
        return;
    }
    const std::array<std::uint8_t, 3> data{'a', 'b', 'c'};
    SCI_CHECK(!sci::vector::hints_match(*hints, data));
    // SHA-256 of "abc" (FIPS 180-2).
    const auto abc = parse_hints(replaced(valid, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                                          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    SCI_CHECK(abc.has_value() && sci::vector::hints_match(*abc, data));
    SCI_CHECK(sci::vector::hint_file_name(5) == "pic.005.json");
}

// The scene model of format version 2 (docs/SPEC_ADDENDA.md A4, A.1 to A.7).

namespace {

std::string scene_example() {
    std::ifstream file(std::filesystem::path(SCI_TEST_HINTS_DIR) / "pic.900.json", std::ios::binary);
    SCI_CHECK(file.good());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The error of a file that must be rejected, or "" if it was accepted.
std::string rejection(const std::string& text) {
    const auto hints = parse_hints(text);
    return hints ? std::string() : hints.error().message;
}

} // namespace

SCI_TEST(hints_read_the_scene_model) {
    using namespace sci::vector;
    const auto hints = parse_hints(scene_example());
    SCI_CHECK(hints.has_value());
    if (!hints) {
        return;
    }
    SCI_CHECK(hints->version == 2 && hints->picture == 900);
    SCI_CHECK(hints->objects.size() == 5 && hints->chains.size() == 1);
    const ObjectHint& window = hints->objects[0];
    SCI_CHECK(window.id == "window-left" && window.main.id == "window-left" && window.cell == "B2.1");
    SCI_CHECK(window.main.type == ObjectType::Window && window.main.confidence == 900);
    SCI_CHECK(window.label == "left window" && window.alternatives.empty() && !window.decision);
    SCI_CHECK(window.main.parts.size() == 3 && window.main.parts[1].role == PartRole::GlazingBar);
    const PrimitiveHint& outline = window.main.parts[0].primitives[0];
    SCI_CHECK(outline.kind == PrimitiveKind::Polyline && outline.commands.size() == 4 && outline.commands[3] == 13);
    SCI_CHECK(outline.points.size() == 5 && outline.points[2] == (Point1x{.x = 90, .y = 100}));
    SCI_CHECK(outline.corners.size() == 3 && outline.corners[2].point == 3);
    SCI_CHECK(outline.corners[2].kind == CornerKind::Sharp);
    SCI_CHECK(window.main.relations.size() == 3);
    SCI_CHECK(window.main.relations[0].kind == GeometricKind::Perpendicular);
    SCI_CHECK(window.main.relations[0].axis == Axis::None);
    SCI_CHECK(window.main.relations[2].kind == GeometricKind::Symmetry);
    SCI_CHECK(window.main.relations[2].axis == Axis::Vertical && window.main.relations[2].items[1] == "wl-sill");
    const ObjectHint& door = hints->objects[2];
    SCI_CHECK(door.cell == "B3.3-C3.1" && door.main.parts[1].primitives[0].kind == PrimitiveKind::Arc);
    SCI_CHECK(door.main.parts[1].primitives[0].corners[1].kind == CornerKind::Rounded);
    SCI_CHECK(door.main.relations[0].kind == GeometricKind::Tangent);
    SCI_CHECK(door.alternatives.size() == 1 && door.alternatives[0].id == "door-pointed");
    SCI_CHECK(door.alternatives[0].confidence == 400 && !door.alternatives[0].note.empty());
    SCI_CHECK(door.decision && door.decision->hypothesis == "door" && door.decision->date == "2026-10-30");
    SCI_CHECK(door.decision && door.decision->cell == "B3.3" && door.decision->note == "round");
    const ObjectHint& oak = hints->objects[3];
    SCI_CHECK(oak.main.type == ObjectType::Tree && is_organic(oak.main.type) && !is_organic(door.main.type));
    SCI_CHECK(oak.main.parts[0].primitives[1].corners[0].kind == CornerKind::Step);
    SCI_CHECK(hints->repetitions.size() == 1 && hints->repetitions[0].items[1] == "window-right");
    const RepetitionHint& row = hints->repetitions[0];
    SCI_CHECK(row.cell == "B2" && row.confidence == 650 && row.note == "same window twice");
    SCI_CHECK(row.decision && row.decision->hypothesis == "repeat" && row.decision->cell == "B2.1-B3.2");
    SCI_CHECK(!is_open(row) && reconstructs_identically(row));
}

// A.7: the customer may keep the 3b reconstruction ("leave the arch"). The
// object is decided, and no hypothesis of it is followed.
SCI_TEST(hints_read_keep_decisions) {
    using namespace sci::vector;
    const std::string scene = scene_example();
    const auto object = [](const std::string& text, std::string_view id) {
        const auto hints = parse_hints(text);
        SCI_CHECK(hints.has_value());
        if (!hints) {
            return ObjectHint{};
        }
        const auto it = std::ranges::find(hints->objects, id, &ObjectHint::id);
        SCI_CHECK(it != hints->objects.end());
        return it == hints->objects.end() ? ObjectHint{} : *it;
    };
    // The example's transom is decided "keep".
    const ObjectHint transom = object(scene, "transom");
    SCI_CHECK(transom.decision && transom.decision->hypothesis == decision_keep);
    SCI_CHECK(transom.decision && transom.decision->cell == "C3" && transom.decision->note == "leave it");
    SCI_CHECK(keeps_reconstruction(transom) && !is_open(transom) && followed_hypothesis(transom) == nullptr);
    // Without the decision it is open, and follows nothing either.
    const std::string keep =
        ",\n      "
        R"("decision": {"hypothesis": "keep", "by": "customer", "date": "2026-11-04", "cell": "C3", "note": "leave it"})";
    const ObjectHint open = object(replaced(scene, keep, ""), "transom");
    SCI_CHECK(!open.decision && is_open(open) && !keeps_reconstruction(open) && followed_hypothesis(open) == nullptr);
    // Decided for a hypothesis, it follows that one.
    const ObjectHint main = object(replaced(scene, R"("hypothesis": "keep")", R"("hypothesis": "transom")"), "transom");
    SCI_CHECK(!is_open(main) && !keeps_reconstruction(main) && followed_hypothesis(main) == &main.main);
    const ObjectHint flat =
        object(replaced(scene, R"("hypothesis": "keep")", R"("hypothesis": "transom-flat")"), "transom");
    SCI_CHECK(followed_hypothesis(flat) == &flat.alternatives[0]);
    // The decided and certain objects of the example.
    const ObjectHint door = object(scene, "door");
    SCI_CHECK(!is_open(door) && !keeps_reconstruction(door) && followed_hypothesis(door) == &door.main);
    const ObjectHint window = object(scene, "window-left");
    SCI_CHECK(!is_open(window) && followed_hypothesis(window) == &window.main);
    // "keep" may also override a certain object.
    const ObjectHint kept_window = object(
        replaced(
            scene, R"("label": "left window",)",
            R"("label": "left window", "decision": {"hypothesis": "keep", "by": "customer", "date": "2026-11-04", "cell": "B2"},)"),
        "window-left");
    SCI_CHECK(keeps_reconstruction(kept_window) && followed_hypothesis(kept_window) == nullptr);
    // "keep" is reserved: no object or alternative is named so.
    SCI_CHECK(rejection(replaced(replaced(scene, R"("id": "transom-flat")", R"("id": "keep")"), keep, ""))
                  .contains("reserved"));
    SCI_CHECK(
        rejection(replaced(replaced(scene, R"("id": "transom",)", R"("id": "keep",)"), keep, "")).contains("reserved"));
    // Other names are still checked; a repetition is decided "repeat" or
    // "as-drawn", never "keep".
    SCI_CHECK(
        rejection(replaced(scene, R"("hypothesis": "keep")", R"("hypothesis": "kept")")).contains("no hypothesis"));
    SCI_CHECK(rejection(replaced(scene, R"("hypothesis": "repeat")", R"("hypothesis": "keep")"))
                  .contains("\"repeat\" or \"as-drawn\" expected"));
}

// A.5 with A.7: a repetition the analyser is unsure of is open until the
// customer decides, and is drawn as drawn meanwhile.
SCI_TEST(hints_read_uncertain_repetitions) {
    using namespace sci::vector;
    const std::string scene = scene_example();
    const std::string decision =
        R"(, "decision": {"hypothesis": "repeat", "by": "customer", "date": "2026-11-04", "cell": "B2.1-B3.2"})";
    const auto read = [](const std::string& text) {
        const auto hints = parse_hints(text);
        SCI_CHECK(hints.has_value() && hints->repetitions.size() == 1);
        return hints && hints->repetitions.size() == 1 ? hints->repetitions[0] : RepetitionHint{};
    };
    // Open: uncertain, no decision.
    const RepetitionHint open = read(replaced(scene, decision, ""));
    SCI_CHECK(open.confidence == 650 && !open.decision && is_open(open) && !reconstructs_identically(open));
    // Certain: no confidence in the file, or 1000.
    const RepetitionHint certain = read(replaced(replaced(scene, decision, ""), R"("confidence": 650,)", ""));
    SCI_CHECK(certain.confidence == 1000 && !is_open(certain) && reconstructs_identically(certain));
    const RepetitionHint sure =
        read(replaced(replaced(scene, decision, ""), R"("confidence": 650)", R"("confidence": 1000)"));
    SCI_CHECK(!is_open(sure) && reconstructs_identically(sure));
    // Decided as drawn.
    const RepetitionHint as_drawn = read(replaced(scene, R"("hypothesis": "repeat")", R"("hypothesis": "as-drawn")"));
    SCI_CHECK(as_drawn.decision && !is_open(as_drawn) && !reconstructs_identically(as_drawn));
    // A decision on a certain repetition is allowed and overrides it.
    const RepetitionHint overridden = read(replaced(replaced(scene, R"("confidence": 650)", R"("confidence": 1000)"),
                                                    R"("hypothesis": "repeat")", R"("hypothesis": "as-drawn")"));
    SCI_CHECK(overridden.confidence == 1000 && !reconstructs_identically(overridden));
    // Invalid.
    SCI_CHECK(rejection(replaced(scene, R"("hypothesis": "repeat")", R"("hypothesis": "window-left")"))
                  .contains("\"repeat\" or \"as-drawn\" expected"));
    SCI_CHECK(
        rejection(replaced(scene, R"("hypothesis": "repeat")", R"("hypothesis": "identical")")).contains("expected"));
    SCI_CHECK(rejection(replaced(scene, R"("confidence": 650)", R"("confidence": 1001)")).contains("not in 0..1000"));
    SCI_CHECK(rejection(replaced(scene, R"("date": "2026-11-04")", R"("date": "2026-11-31")")).contains("date"));
    SCI_CHECK(rejection(replaced(scene, R"("cell": "B2.1-B3.2")", R"("cell": "B6")")).contains("review grid"));
    SCI_CHECK(rejection(replaced(scene, R"("cell": "B2",)", R"("cell": "B2-",)")).contains("review grid"));
    SCI_CHECK(read(replaced(scene, R"("cell": "B2",)", "")).cell.empty());
    SCI_CHECK(rejection(replaced(scene, R"("by": "customer", "date": "2026-11-04")",
                                 R"("by": "analyser", "date": "2026-11-04")"))
                  .contains("customer"));
    SCI_CHECK(rejection(replaced(scene, decision, R"(, "decided": "repeat")")).contains("unknown member"));
    // The existing rules hold for open repetitions too.
    const std::string open_scene = replaced(scene, decision, "");
    SCI_CHECK(rejection(replaced(open_scene, R"(["window-left", "window-right"])", R"(["window-left", "door"])"))
                  .contains("one type"));
    SCI_CHECK(rejection(replaced(replaced(open_scene, R"("type": "tree")", R"("type": "window")"),
                                 R"(["window-left", "window-right"])", R"(["window-left", "oak"])"))
                  .empty());
    SCI_CHECK(rejection(replaced(replaced(open_scene, R"("role": "trunk")", R"("role": "frame")"),
                                 R"(["window-left", "window-right"])", R"(["oak-trunk", "wl-frame"])"))
                  .contains("organic"));
}

// A version 1 file reads as before, and as the same file with version 2;
// the scene model needs version 2.
SCI_TEST(hints_migrate_version_1) {
    const auto old = parse_hints(valid);
    const auto migrated = parse_hints(replaced(valid, R"("version": 1)", R"("version": 2)"));
    SCI_CHECK(old.has_value() && migrated.has_value());
    if (!old || !migrated) {
        return;
    }
    SCI_CHECK(old->version == 1 && migrated->version == 2);
    SCI_CHECK(old->objects.empty() && old->repetitions.empty() && migrated->objects.empty());
    SCI_CHECK(old->pic_sha256 == migrated->pic_sha256 && old->analyser == migrated->analyser);
    SCI_CHECK(old->regions.size() == migrated->regions.size() && old->chains.size() == migrated->chains.size());
    for (std::size_t i = 0; i < old->chains.size() && i < migrated->chains.size(); ++i) {
        SCI_CHECK(old->chains[i].lines == migrated->chains[i].lines);
        SCI_CHECK(old->chains[i].primitive == migrated->chains[i].primitive);
    }
    SCI_CHECK(old->gaps.size() == migrated->gaps.size() && old->symmetry.size() == migrated->symmetry.size());
    SCI_CHECK(old->relations.size() == migrated->relations.size());
    const std::string scene = scene_example();
    SCI_CHECK(rejection(replaced(scene, R"("version": 2)", R"("version": 1)")).contains("need version 2"));
    SCI_CHECK(
        rejection(replaced(valid, R"("version": 1)", R"("version": 1, "repetitions": [])")).contains("need version 2"));
    SCI_CHECK(!rejection(replaced(scene, R"("version": 2)", R"("version": 3)")).empty());
}

SCI_TEST(hints_reject_invalid_scene_models) {
    // From, to, and part of the error message.
    using Breakage = std::tuple<const char*, const char*, const char*>;
    const std::array<Breakage, 44> breakages{
        {
            // Vocabulary and values.
            {R"("type": "tree")", R"("type": "bush")", "one of"},
            {R"("role": "sill")", R"("role": "ledge")", "one of"},
            {R"("kind": "step")", R"("kind": "jagged")", "one of"},
            {R"("kind": "perpendicular")", R"("kind": "orthogonal")", "one of"},
            {R"("axis": "vertical")", R"("axis": "diagonal")", "one of"},
            {R"("confidence": 900)", R"("confidence": 1001)", "not in 0..1000"},
            {R"("confidence": 900)", R"("confidence": -1)", "not in 0..1000"},
            {R"("cell": "B2.1")", R"("cell": "D2.1")", "review grid"},
            {R"("cell": "B2.1")", R"("cell": "B2.5")", "review grid"},
            {R"("cell": "B2.1")", R"("cell": "b2.1")", "review grid"},
            {R"("cell": "A1-B1")", R"("cell": "A1-")", "review grid"},
            {R"("cell": "A1-B1")", R"("cell": "A1 - B1")", "review grid"},
            {R"("corners": [{"point": 0)", R"("corner": [{"point": 0)", "unknown member"},
            {R"("note": "a pointed arch)", R"("label": "a pointed arch)", "unknown member"},
            // Anchors.
            {"[10, 11, 12, 13]", "[10, 11, 11, 13]", "listed twice"},
            {R"("commands": [53])", R"("commands": [1000001])", "not in 0..1000000"},
            {R"("commands": [53])", R"("commands": [])", "items expected"},
            {"[[70, 85], [90, 85]]", "[[70, 85], [320, 85]]", "not in 0..319"},
            {"[[70, 85], [90, 85]]", "[[70, 85], [90, 190]]", "not in 0..189"},
            {"[[80, 70], [80, 100]]", "[[80, 70], [80, 85], [80, 100]]", "points expected"},
            {R"("kind": "arc")", R"("kind": "line")", "points expected"},
            {R"({"point": 3, "kind": "sharp"})", R"({"point": 5, "kind": "sharp"})", "not in 0..4"},
            {R"({"point": 2, "kind": "sharp"})", R"({"point": 1, "kind": "sharp"})", "two corners"},
            // Ids and references.
            {R"("id": "oak-bark-step")", R"("id": "door-arch-chain")", "used twice"},
            {R"("id": "door-pointed")", R"("id": "window-left")", "used twice"},
            {R"("id": "wr-frame")", R"("id": "wl-frame")", "used twice"},
            {R"("id": "windows-row")", R"("id": "oak")", "used twice"},
            {R"(["wl-bar-v", "wl-bar-h"])", R"(["wl-bar-v", "nothing"])", "no part or primitive"},
            {R"(["wl-bar-h", "wl-sill-top"])", R"(["wl-bar-h", "door-jamb-left"])", "no part or primitive"},
            {R"(["wl-bar-h", "wl-sill-top"])", R"(["wl-bar-h", "wl-bar-h"])", "listed twice"},
            {R"(["wl-bar-v", "wl-bar-h"])", R"(["wl-bar-v", "wl-bar-h", "wl-sill-top"])", "two items"},
            {R"("kind": "parallel")", R"("kind": "equal-spacing")", "three or more"},
            {R"(, "axis": "vertical")", "", "an axis"},
            {R"(["wl-bar-h", "wl-sill-top"]})", R"(["wl-bar-h", "wl-sill-top"], "axis": "vertical"})", "an axis"},
            {R"(["door-arch-curve", "door-jamb-left"])", R"(["door-jamb-right", "door-jamb-left"])", "or ellipse"},
            {R"(["door-arch-curve", "door-jamb-left"])", R"(["door-arch-curve", "door-jambs"])", "or ellipse"},
            {R"("hypothesis": "door")", R"("hypothesis": "window-left")", "no hypothesis"},
            {R"("by": "customer")", R"("by": "agent")", "customer"},
            {R"("date": "2026-10-30")", R"("date": "2026-02-29")", "date"},
            {R"("date": "2026-10-30")", R"("date": "26-10-30")", "date"},
            {R"(["window-left", "window-right"])", R"(["window-left", "nothing"])", "no object or part"},
            {R"(["window-left", "window-right"])", R"(["window-left", "door"])", "one type"},
            {R"(["window-left", "window-right"])", R"(["window-left", "wl-frame"])", "not both"},
            {
                R"("cell": "B2.1-B3.2"})",
                R"("cell": "B2.1-B3.2"}}, {"id": "again", "items": ["window-right", "window-left"])",
                "two repetitions",
            },
        },
    };
    const std::string scene = scene_example();
    for (const auto& [from, to, error] : breakages) {
        const std::string message = rejection(replaced(scene, from, to));
        SCI_CHECK(message.contains(error));
    }
    // A.4: organic objects get no idealisation.
    SCI_CHECK(rejection(replaced(scene, R"("kind": "spline")", R"("kind": "ellipse")")).contains("organic"));
    SCI_CHECK(rejection(replaced(scene, R"("cell": "A1-B1",)",
                                 R"("cell": "A1-B1", "relations": [{"kind": "parallel", "items": ["oak-trunk-left",
                                    "oak-bark-step"]}],)"))
                  .contains("organic"));
    SCI_CHECK(rejection(replaced(replaced(scene, R"("type": "tree")", R"("type": "window")"),
                                 R"(["window-left", "window-right"])", R"(["window-left", "oak"])"))
                  .empty());
    SCI_CHECK(rejection(replaced(replaced(scene, R"("role": "trunk")", R"("role": "frame")"),
                                 R"(["window-left", "window-right"])", R"(["oak-trunk", "wl-frame"])"))
                  .contains("organic"));
    // A valid leap day.
    SCI_CHECK(rejection(replaced(scene, R"("date": "2026-10-30")", R"("date": "2028-02-29")")).empty());
}

// A.3, the two choices in relation solving (docs/ASSUMPTIONS.md): the
// customer's choice of the main hypothesis over alternatives keeps the
// relations unsolved (as drawn); a chosen alternative has them solved.
SCI_TEST(relations_stay_as_drawn_only_for_a_main_hypothesis_chosen_over_alternatives) {
    using sci::vector::Decision;
    using sci::vector::Hypothesis;
    using sci::vector::ObjectHint;
    ObjectHint window{.id = "window", .cell = "B3", .label = "window", .main = {}, .alternatives = {}, .decision = {}};
    window.main.id = "window";
    Hypothesis equal;
    equal.id = "window-equal-panes";
    // Certain (no alternatives, no decision): solved.
    SCI_CHECK(!sci::vector::keeps_relations_as_drawn(window));
    // Decided without alternatives (the rhombus): solved.
    window.decision = Decision{.hypothesis = "window", .date = "2026-11-04", .cell = "B3", .note = ""};
    SCI_CHECK(!sci::vector::keeps_relations_as_drawn(window));
    // The main hypothesis chosen over an alternative: as drawn, unsolved.
    window.alternatives.push_back(equal);
    SCI_CHECK(sci::vector::keeps_relations_as_drawn(window));
    SCI_CHECK(sci::vector::followed_hypothesis(window) == &window.main);
    // The alternative chosen: its relations are solved.
    window.decision->hypothesis = "window-equal-panes";
    SCI_CHECK(!sci::vector::keeps_relations_as_drawn(window));
    SCI_CHECK(sci::vector::followed_hypothesis(window) == &window.alternatives[0]);
    // Open (alternatives, no decision): nothing is followed at all.
    window.decision.reset();
    SCI_CHECK(!sci::vector::keeps_relations_as_drawn(window) && sci::vector::followed_hypothesis(window) == nullptr);
}
