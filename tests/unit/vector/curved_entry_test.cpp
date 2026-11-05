// SPDX-License-Identifier: GPL-3.0-or-later
//
// A steered closed ellipse through the whole scene building
// (docs/hd-pipeline.md, "Curved shapes in the arrangement" and "Room
// entry"): the fast analysis gives the reference's scene bit for bit, the
// stroke and both faces share the curve, and along it every HD pixel's
// 16 x 16 samples belong to exactly one face each.

#include "pic_builder.hpp"
#include "sci/pic/draw_pic.hpp"
#include "sci/pic/parse_pic.hpp"
#include "sci/render/rasterize.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci/vector/scene_io.hpp"
#include "sci_test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using sci::test::PicBuilder;
using sci::vector::Analysis;
using sci::vector::Plane;
using sci::vector::Vec2;

namespace {

struct Drawn {
    sci::pic::PictureMaps maps;
    sci::pic::DrawTrace trace;
    std::vector<sci::vector::LineRef> lines; // The command of each line, in drawing order.
    std::size_t oval = 0;                    // The oval's first line.
    std::vector<std::int32_t> oval_sources;  // The oval's lines in the trace.
};

struct Point {
    int x;
    int y;
};

// Points on the ellipse around (100, 80) with semi-axes 20 and 12, every
// 22.5 degrees, rounded to pixels.
constexpr std::array<Point, 16> oval_points{
    Point{.x = 120, .y = 80}, Point{.x = 118, .y = 85}, Point{.x = 114, .y = 88}, Point{.x = 108, .y = 91},
    Point{.x = 100, .y = 92}, Point{.x = 92, .y = 91},  Point{.x = 86, .y = 88},  Point{.x = 82, .y = 85},
    Point{.x = 80, .y = 80},  Point{.x = 82, .y = 75},  Point{.x = 86, .y = 72},  Point{.x = 92, .y = 69},
    Point{.x = 100, .y = 68}, Point{.x = 108, .y = 69}, Point{.x = 114, .y = 72}, Point{.x = 118, .y = 75},
};
constexpr Point line_end_from{.x = 132, .y = 86};
constexpr Point line_end_to{.x = 119, .y = 86};

// An oval of 16 lines filled with one colour, in a field of another; with
// `with_line_end`, a short line ending next to it.
Drawn oval_picture(bool with_line_end) {
    PicBuilder pic;
    pic.color(0).rectangle(10, 10, 300, 180);
    pic.color(12);
    for (std::size_t k = 0; k < oval_points.size(); ++k) {
        const Point a = oval_points[k];
        const Point b = oval_points[(k + 1) % oval_points.size()];
        pic.line(a.x, a.y, b.x, b.y);
    }
    if (with_line_end) {
        pic.color(0).line(line_end_from.x, line_end_from.y, line_end_to.x, line_end_to.y);
    }
    pic.color(17).fill(100, 80).color(3).fill(50, 30);
    Drawn drawn;
    const auto commands = sci::pic::parse_pic(pic.end());
    SCI_CHECK(commands.has_value());
    if (commands) {
        SCI_CHECK(sci::pic::draw_picture(*commands, drawn.maps, {}, &drawn.trace).has_value());
    }
    for (const auto& primitive : drawn.trace.primitives) {
        if (std::holds_alternative<sci::pic::Line>(primitive.shape)) {
            drawn.lines.push_back(primitive.command);
        }
    }
    // The rectangle's lines come first.
    drawn.oval = drawn.lines.size() - oval_points.size() - (with_line_end ? 1 : 0);
    std::size_t line = 0;
    for (std::size_t i = 0; i < drawn.trace.primitives.size(); ++i) {
        if (std::holds_alternative<sci::pic::Line>(drawn.trace.primitives[i].shape)) {
            if (line >= drawn.oval && line < drawn.oval + oval_points.size()) {
                drawn.oval_sources.push_back(static_cast<std::int32_t>(i));
            }
            ++line;
        }
    }
    return drawn;
}

// The oval as a decided scene-model object: one closed ellipse.
sci::vector::PictureHints oval_hints(const Drawn& drawn) {
    using namespace sci::vector;
    const auto first = drawn.lines.begin() + static_cast<std::ptrdiff_t>(drawn.oval);
    const auto last = first + static_cast<std::ptrdiff_t>(oval_points.size());
    PrimitiveHint oval{
        .id = "oval",
        .kind = PrimitiveKind::Ellipse,
        .commands = {first, last},
        .points = {},
        .corners = {},
    };
    oval.points = {
        {.x = 120, .y = 80}, {.x = 100, .y = 92}, {.x = 80, .y = 80}, {.x = 100, .y = 68}, {.x = 120, .y = 80},
    };
    PartHint outline{.id = "outline", .role = PartRole::Outline, .label = {}, .primitives = {}};
    outline.primitives.push_back(oval);
    Hypothesis medallion{
        .id = "medallion",
        .type = ObjectType::Other,
        .confidence = 1000,
        .note = {},
        .parts = {},
        .relations = {},
    };
    medallion.parts.push_back(outline);
    ObjectHint object{
        .id = "medallion",
        .cell = "B2",
        .label = "an oval medallion",
        .main = medallion,
        .alternatives = {},
        .decision = std::nullopt,
    };
    object.decision = Decision{.hypothesis = "medallion", .date = "2026-11-05", .cell = "B2", .note = {}};
    PictureHints hints;
    hints.version = 2;
    hints.objects.push_back(object);
    return hints;
}

sci::vector::BuiltScene build(const Drawn& drawn, const sci::vector::PictureHints& hints, Analysis analysis) {
    const std::array<const sci::vector::PictureHints*, 1> per_picture{&hints};
    return sci::vector::build_scene(drawn.maps, drawn.trace, Plane::Visual, per_picture, analysis);
}

std::string outcome(const sci::vector::BuiltScene& built) {
    for (const auto& hint : built.stats.applied_hints) {
        if (hint.id == "oval") {
            return hint.detail;
        }
    }
    return {};
}

// The stroke of the oval's fitted lines, as one closed polyline.
std::vector<Vec2> oval_stroke(const Drawn& drawn, const sci::vector::Scene& scene) {
    std::vector<Vec2> points;
    for (const auto& layer : scene.layers) {
        if (const auto* stroke = std::get_if<sci::vector::Stroke>(&layer);
            stroke != nullptr && std::ranges::find(drawn.oval_sources, stroke->source) != drawn.oval_sources.end()) {
            for (const Vec2 p : stroke->points) {
                if (points.empty() || std::abs(points.back().x - p.x) > 1e-9 ||
                    std::abs(points.back().y - p.y) > 1e-9) {
                    points.push_back(p);
                }
            }
        }
    }
    return points;
}

double distance_to(const std::vector<Vec2>& polyline, Vec2 p) {
    double best = 1e9;
    for (std::size_t k = 0; k + 1 < polyline.size(); ++k) {
        const Vec2 a = polyline[k];
        const Vec2 d{.x = polyline[k + 1].x - a.x, .y = polyline[k + 1].y - a.y};
        const double len2 = (d.x * d.x) + (d.y * d.y);
        const double t = len2 > 0 ? std::clamp((((p.x - a.x) * d.x) + ((p.y - a.y) * d.y)) / len2, 0.0, 1.0) : 0.0;
        const double dx = a.x + (t * d.x) - p.x;
        const double dy = a.y + (t * d.y) - p.y;
        best = std::min(best, std::sqrt((dx * dx) + (dy * dy)));
    }
    return best;
}

bool inside(const std::vector<Vec2>& polygon, Vec2 p);

// What the samples of the pixels the curve passes through show (N = 1,
// 16 x 16 samples per pixel).
struct Coverage {
    int pixels = 0;     // Pixels the curve passes through.
    int wrong = 0;      // Samples farther than 1/32 pixel from it in the other side's face.
    int incomplete = 0; // Pixels with samples in neither the inside nor the outside face.
    int one_sided = 0;  // Pixels the curve passes near the centre of with samples of one face only.
};

constexpr std::size_t density = 16;

// The value of a face, -1 for none.
int value_of(const sci::vector::Scene& scene, std::int32_t face) {
    return face < 0 ? -1 : static_cast<int>(scene.faces[static_cast<std::size_t>(face)].value);
}

// One pixel's samples (rows: the 16 rows of samples of its pixel row).
void cover_pixel(const sci::vector::Scene& scene, const std::vector<Vec2>& stroke,
                 const std::vector<std::vector<std::int32_t>>& rows, int x, int y, std::array<int, 2> values,
                 Coverage& coverage) {
    constexpr double off_curve = (1.0 / 32) + 1e-6;
    std::array<int, 2> count{};
    for (std::size_t sy = 0; sy < density; ++sy) {
        for (std::size_t sx = 0; sx < density; ++sx) {
            const int value = value_of(scene, rows[sy][(static_cast<std::size_t>(x) * density) + sx]);
            count[0] += value == values[0] ? 1 : 0;
            count[1] += value == values[1] ? 1 : 0;
            const Vec2 p{
                .x = x + ((static_cast<double>(sx) + 0.5) / density),
                .y = y + ((static_cast<double>(sy) + 0.5) / density),
            };
            if (distance_to(stroke, p) > off_curve && value != values[inside(stroke, p) ? 0 : 1]) {
                ++coverage.wrong;
            }
        }
    }
    coverage.incomplete += std::cmp_equal(count[0] + count[1], density * density) ? 0 : 1;
    if (distance_to(stroke, {.x = x + 0.5, .y = y + 0.5}) < 0.25 && (count[0] == 0 || count[1] == 0)) {
        ++coverage.one_sided;
    }
}

// The samples of every pixel the curve passes through; values: the
// inside's, the outside's.
Coverage cover(const sci::vector::Scene& scene, const std::vector<Vec2>& stroke, std::array<int, 2> values) {
    const sci::render::SceneSampler sampler(scene);
    Coverage coverage;
    std::vector<std::int32_t> row(static_cast<std::size_t>(sci::vector::picture_width) * density);
    for (int y = 64; y < 97; ++y) {
        std::vector<std::vector<std::int32_t>> rows;
        for (std::size_t s = 0; s < density; ++s) {
            sampler.sample_faces(y + ((static_cast<double>(s) + 0.5) / density), density, row);
            rows.push_back(row);
        }
        for (int x = 76; x < 125; ++x) {
            if (distance_to(stroke, {.x = x + 0.5, .y = y + 0.5}) <= 0.75) {
                ++coverage.pixels;
                cover_pixel(scene, stroke, rows, x, y, values, coverage);
            }
        }
    }
    return coverage;
}

bool inside(const std::vector<Vec2>& polygon, Vec2 p) {
    bool in = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < a.x + ((p.y - a.y) * (b.x - a.x) / (b.y - a.y))) {
            in = !in;
        }
    }
    return in;
}

} // namespace

SCI_TEST(a_steered_oval_builds_the_same_scene_in_both_analyses) {
    for (const bool with_line_end : {false, true}) {
        const Drawn drawn = oval_picture(with_line_end);
        const sci::vector::PictureHints hints = oval_hints(drawn);
        const auto fast = build(drawn, hints, Analysis::Fast);
        const auto reference = build(drawn, hints, Analysis::Reference);
        SCI_CHECK(!outcome(fast).empty());
        SCI_CHECK(outcome(fast) == outcome(reference));
        SCI_CHECK(sci::vector::write_scene(fast.scene) == sci::vector::write_scene(reference.scene));
    }
}

// The stroke is the steered ellipse, and the faces inside and outside meet
// on it: at N = 1 every pixel the curve passes through has its 16 x 16
// samples in the two faces only (256 in all, none in a third face or in
// none), each sample farther than 1/32 pixel from the stroke in the face of
// its side (the chords of a curve in the arrangement are at most 1/32
// pixel off; a curve shared by moving vertices is the faces' course), and
// both faces where the curve runs near the pixel's centre.
SCI_TEST(the_oval_s_stroke_and_faces_share_its_curve) {
    const Drawn drawn = oval_picture(false);
    const auto built = build(drawn, oval_hints(drawn), Analysis::Fast);
    SCI_CHECK(outcome(built).contains("shared by the faces"));
    const sci::vector::Scene& scene = built.scene;
    const std::vector<Vec2> stroke = oval_stroke(drawn, scene);
    SCI_CHECK(stroke.size() > oval_points.size());
    if (stroke.size() <= oval_points.size()) {
        return;
    }
    // The stroke runs on the ellipse, within the round trip.
    for (const Vec2 p : stroke) {
        const double dx = (p.x - 100.5) / 20;
        const double dy = (p.y - 80.5) / 12;
        SCI_CHECK(std::abs(std::sqrt((dx * dx) + (dy * dy)) - 1) < 0.08);
    }
    const sci::render::SceneSampler sampler(scene);
    std::vector<std::int32_t> row(static_cast<std::size_t>(sci::vector::picture_width) * density);
    sampler.sample_faces(80.5, density, row);
    const int inner = value_of(scene, row[(100 * density) + (density / 2)]);
    const int outer = value_of(scene, row[(50 * density) + (density / 2)]);
    SCI_CHECK(inner != outer && inner >= 0 && outer >= 0);
    const Coverage coverage = cover(scene, stroke, {inner, outer});
    SCI_CHECK(coverage.pixels > 100);
    SCI_CHECK(coverage.incomplete == 0);
    SCI_CHECK(coverage.wrong == 0);
    SCI_CHECK(coverage.one_sided == 0);
}
