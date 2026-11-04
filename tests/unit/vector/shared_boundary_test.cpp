// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shared boundary of steered shapes (spec section 6.6;
// docs/hd-pipeline.md, "The shared boundary"): synthetic arrangements.

#include "curves.hpp"
#include "sci_test.hpp"
#include "shared_boundary.hpp"
#include "topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using sci::core::IntPoint;
using sci::vector::Vec2;
using sci::vector::detail::Arrangement;
using sci::vector::detail::BoundaryLine;
using sci::vector::detail::build_arrangement;
using sci::vector::detail::FittedChain;
using sci::vector::detail::make_segment;
using sci::vector::detail::SharedBoundary;
using sci::vector::detail::TopoSegment;

namespace {

// The unit circle in 16 steps (no trigonometry in the tests).
constexpr std::array<Vec2, 16> unit{
    Vec2{.x = 1, .y = 0},
    Vec2{.x = 0.92388, .y = 0.38268},
    Vec2{.x = 0.70711, .y = 0.70711},
    Vec2{.x = 0.38268, .y = 0.92388},
    Vec2{.x = 0, .y = 1},
    Vec2{.x = -0.38268, .y = 0.92388},
    Vec2{.x = -0.70711, .y = 0.70711},
    Vec2{.x = -0.92388, .y = 0.38268},
    Vec2{.x = -1, .y = 0},
    Vec2{.x = -0.92388, .y = -0.38268},
    Vec2{.x = -0.70711, .y = -0.70711},
    Vec2{.x = -0.38268, .y = -0.92388},
    Vec2{.x = 0, .y = -1},
    Vec2{.x = 0.38268, .y = -0.92388},
    Vec2{.x = 0.70711, .y = -0.70711},
    Vec2{.x = 0.92388, .y = -0.38268},
};

constexpr Vec2 centre{.x = 20.5, .y = 15.5};
constexpr double rx = 9.0;
constexpr double ry = 6.0;

Vec2 on_ellipse(Vec2 u) {
    return {.x = centre.x + (rx * u.x), .y = centre.y + (ry * u.y)};
}

// A point to the nearest pixel centre.
Vec2 to_pixel_centre(Vec2 p) {
    return {.x = std::floor(p.x) + 0.5, .y = std::floor(p.y) + 0.5};
}

IntPoint doubled(Vec2 p) {
    return {.x = static_cast<std::int64_t>(2 * p.x), .y = static_cast<std::int64_t>(2 * p.y)};
}

double distance(Vec2 a, Vec2 b) {
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)));
}

double length(const std::vector<Vec2>& course) {
    double total = 0.0;
    for (std::size_t k = 0; k + 1 < course.size(); ++k) {
        total += distance(course[k], course[k + 1]);
    }
    return total;
}

// The distance from p to a polyline.
double off(const std::vector<Vec2>& polyline, Vec2 p) {
    double best = distance(p, polyline.front());
    for (std::size_t k = 0; k + 1 < polyline.size(); ++k) {
        const Vec2 a = polyline[k];
        const Vec2 b = polyline[k + 1];
        const double len2 = ((b.x - a.x) * (b.x - a.x)) + ((b.y - a.y) * (b.y - a.y));
        const double f = len2 == 0.0
                             ? 0.0
                             : std::clamp((((p.x - a.x) * (b.x - a.x)) + ((p.y - a.y) * (b.y - a.y))) / len2, 0.0, 1.0);
        best = std::min(best, distance(p, Vec2{.x = a.x + (f * (b.x - a.x)), .y = a.y + (f * (b.y - a.y))}));
    }
    return best;
}

// A closed ellipse drawn as eight straight lines between pixel centres,
// steered back onto the ellipse: each line's piece is the ellipse between
// its corners.
struct ClosedEllipse {
    std::vector<TopoSegment> segments;
    std::vector<BoundaryLine> lines;
};

ClosedEllipse closed_ellipse() {
    ClosedEllipse shape;
    constexpr std::size_t corners = 8;
    for (std::size_t i = 0; i < corners; ++i) {
        const std::size_t a = 2 * i;
        const std::size_t b = (2 * (i + 1)) % unit.size();
        const Vec2 from = to_pixel_centre(on_ellipse(unit[a]));
        const Vec2 to = to_pixel_centre(on_ellipse(unit[b]));
        shape.segments.push_back(make_segment(doubled(from), doubled(to), static_cast<std::int32_t>(i)));
        // The piece: the ellipse from corner to corner, through the step
        // between them, with its ends where the lines meet.
        std::vector<Vec2> piece{on_ellipse(unit[a]), on_ellipse(unit[a + 1]), on_ellipse(unit[b])};
        shape.lines.push_back(
            BoundaryLine{.source = static_cast<std::int32_t>(i), .from = from, .to = to, .piece = std::move(piece)});
    }
    return shape;
}

// The courses of every edge, as the scan edges take them.
std::vector<std::vector<Vec2>> courses(const Arrangement& arrangement, const SharedBoundary& boundary) {
    std::vector<std::vector<Vec2>> all;
    for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
        if (const auto& course = boundary.overrides()[e]) {
            all.push_back(*course);
        } else {
            const auto vertex = [&](std::size_t h) {
                return sci::vector::detail::to_picture(
                    arrangement.vertices[static_cast<std::size_t>(arrangement.half_edges[h].origin)]);
            };
            all.push_back({vertex(2 * e), vertex((2 * e) + 1)});
        }
    }
    return all;
}

} // namespace

SCI_TEST(a_steered_closed_ellipse_is_the_boundary_of_both_faces) {
    const ClosedEllipse shape = closed_ellipse();
    const Arrangement arrangement = build_arrangement(shape.segments);
    SharedBoundary boundary(arrangement, std::vector<FittedChain>{});
    SCI_CHECK(boundary.apply(shape.lines).empty());
    SCI_CHECK(boundary.changed());
    const std::vector<std::vector<Vec2>> all = courses(arrangement, boundary);
    // Each line's edges run along its piece (the stroke), and together
    // cover it from end to end: the faces inside and outside both end on
    // the stroke's centreline, with no gap or overlap between them.
    for (const BoundaryLine& line : shape.lines) {
        double covered = 0.0;
        for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
            if (arrangement.sources[e] != line.source) {
                continue;
            }
            SCI_CHECK(boundary.overrides()[e].has_value());
            for (const Vec2 p : all[e]) {
                SCI_CHECK(off(line.piece, p) < 1e-9);
            }
            covered += length(all[e]);
        }
        SCI_CHECK(std::abs(covered - length(line.piece)) < 1e-9);
    }
    // One face inside, one outside: every edge has the inside on one side
    // and the outside on the other (the faces are kept).
    for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
        SCI_CHECK(arrangement.half_edges[2 * e].face != arrangement.half_edges[(2 * e) + 1].face);
    }
}

SCI_TEST(a_shape_the_partition_cannot_take_leaves_the_3b_course_exactly) {
    // Line 0 along y = 0.5, line 1 along y = 1.5; a piece for line 0 that
    // bulges past line 1 would cross it.
    const std::vector<TopoSegment> segments{
        make_segment({.x = 1, .y = 1}, {.x = 21, .y = 1}, 0),
        make_segment({.x = 1, .y = 3}, {.x = 21, .y = 3}, 1),
        make_segment({.x = 1, .y = 1}, {.x = 1, .y = 3}, 2),
        make_segment({.x = 21, .y = 1}, {.x = 21, .y = 3}, 3),
    };
    const Arrangement arrangement = build_arrangement(segments);
    SharedBoundary boundary(arrangement, std::vector<FittedChain>{});
    const std::vector<std::vector<Vec2>> before = courses(arrangement, boundary);
    const BoundaryLine bulge{
        .source = 0,
        .from = {.x = 0.5, .y = 0.5},
        .to = {.x = 10.5, .y = 0.5},
        .piece = {{.x = 0.5, .y = 0.5}, {.x = 5.5, .y = 2.5}, {.x = 10.5, .y = 0.5}},
    };
    SCI_CHECK(!boundary.apply({bulge}).empty());
    SCI_CHECK(!boundary.changed());
    SCI_CHECK(std::ranges::none_of(boundary.overrides(), [](const auto& course) { return course.has_value(); }));
    SCI_CHECK(courses(arrangement, boundary) == before);
    // A shape the partition takes afterwards is applied on the 3b course.
    const BoundaryLine flat{
        .source = 0,
        .from = {.x = 0.5, .y = 0.5},
        .to = {.x = 10.5, .y = 0.5},
        .piece = {{.x = 0.5, .y = 0.5}, {.x = 5.5, .y = 0.75}, {.x = 10.5, .y = 0.5}},
    };
    SCI_CHECK(boundary.apply({flat}).empty());
    const std::vector<std::vector<Vec2>> after = courses(arrangement, boundary);
    for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
        if (arrangement.sources[e] != 0) {
            SCI_CHECK(after[e] == before[e]);
        }
    }
}

SCI_TEST(an_arch_springing_from_its_jamb_is_pushed_just_off_it) {
    // A vertical jamb (x = 2.5, y 0.5 to 6.5) and an arch line leaving its
    // lower end. The arch's piece leaves the end tangent to the jamb and
    // dips 0.03 px to the jamb's other side before turning away; the
    // neighbouring course is pushed back by at most a quarter pixel, and
    // the partition stays valid.
    const std::vector<TopoSegment> segments{
        make_segment({.x = 5, .y = 1}, {.x = 5, .y = 13}, 0),
        make_segment({.x = 5, .y = 13}, {.x = 15, .y = 1}, 1),
        make_segment({.x = 5, .y = 1}, {.x = 15, .y = 1}, 2),
    };
    const Arrangement arrangement = build_arrangement(segments);
    SharedBoundary boundary(arrangement, std::vector<FittedChain>{});
    const std::vector<Vec2> piece{
        {.x = 2.5, .y = 6.5}, {.x = 2.47, .y = 6.0}, {.x = 2.48, .y = 5.5}, {.x = 2.52, .y = 5.0}, {.x = 2.7, .y = 4.0},
        {.x = 3.2, .y = 3.0}, {.x = 4.2, .y = 2.0},  {.x = 5.5, .y = 1.2},  {.x = 7.5, .y = 0.5},
    };
    const BoundaryLine arch{.source = 1, .from = {.x = 2.5, .y = 6.5}, .to = {.x = 7.5, .y = 0.5}, .piece = piece};
    SCI_CHECK(boundary.apply({arch}).empty());
    const std::vector<std::vector<Vec2>> all = courses(arrangement, boundary);
    for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
        if (arrangement.sources[e] == 1) {
            for (const Vec2 p : all[e]) {
                // Within a quarter pixel of the steered piece, and never
                // past the jamb.
                SCI_CHECK(off(piece, p) <= 0.25);
                SCI_CHECK(p.x >= 2.5);
            }
        }
    }
}
