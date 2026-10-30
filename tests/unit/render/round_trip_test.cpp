// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/pic/draw_trace.hpp"
#include "sci/pic/geometry.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci/render/round_trip.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci/vector/scene.hpp"
#include "sci_test.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using sci::vector::Vec2;

namespace {

sci::pic::TracedPrimitive traced_line(int x0, int y0, int x1, int y1) {
    sci::pic::TracedPrimitive primitive;
    primitive.state.planes = sci::pic::plane_visual;
    primitive.shape = sci::pic::Line{
        .from = {.x = static_cast<std::int16_t>(x0), .y = static_cast<std::int16_t>(y0)},
        .to = {.x = static_cast<std::int16_t>(x1), .y = static_cast<std::int16_t>(y1)},
    };
    return primitive;
}

Vec2 centre(int x, int y) {
    return {.x = x + 0.5, .y = y + 0.5};
}

// The polyline from a to b in chords of at most 0.25 pixels, as the fitting
// flattens curves.
std::vector<Vec2> chords(Vec2 a, Vec2 b) {
    const double length = std::sqrt(((b.x - a.x) * (b.x - a.x)) + ((b.y - a.y) * (b.y - a.y)));
    const int steps = static_cast<int>(std::ceil(length / 0.25));
    std::vector<Vec2> points;
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        points.push_back({.x = a.x + ((b.x - a.x) * t), .y = a.y + ((b.y - a.y) * t)});
    }
    return points;
}

// A chain of one fitted line whose stroke is `points`.
sci::vector::SceneStats one_chain(std::vector<std::int32_t> lines, std::vector<std::vector<Vec2>> strokes,
                                  sci::vector::FittedKind kind) {
    sci::vector::SceneStats stats;
    sci::vector::FittedChain chain;
    chain.kind = kind;
    chain.lines = lines;
    stats.fitted_chains.push_back(chain);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        sci::vector::FittedLine line;
        line.primitive = lines[i];
        line.points = std::move(strokes[i]);
        stats.fitted_strokes.push_back(std::move(line));
    }
    return stats;
}

std::vector<std::uint32_t> path1_pixels(int x0, int y0, int x1, int y1) {
    std::vector<std::uint32_t> pixels;
    sci::pic::for_each_line_pixel({.x = x0, .y = y0}, {.x = x1, .y = y1},
                                  [&](int x, int y) { pixels.push_back(static_cast<std::uint32_t>((y * 320) + x)); });
    std::ranges::stable_sort(pixels);
    const auto [first, last] = std::ranges::unique(pixels);
    pixels.erase(first, last);
    return pixels;
}

// The point at parameter t (0 to 1) of the quarter circle of radius r
// counter-clockwise from (r, 0), on screen (y down), in the rational
// parametrisation: no trigonometry (spec section 5.3).
Vec2 on_circle(double r, double t) {
    const double d = 1 + (t * t);
    return {.x = r * (1 - (t * t)) / d, .y = -r * 2 * t / d};
}

// A quarter circle of radius r around pixel (cx, cy) as PIC lines between
// the pixels nearest to it at `steps` equal parameter steps, and the trace
// of those lines.
struct Staircase {
    sci::pic::DrawTrace trace;
    std::vector<std::int32_t> lines;
};

Staircase quarter_circle(int cx, int cy, int r, int steps) {
    Staircase stairs;
    int px = cx + r;
    int py = cy;
    for (int k = 1; k <= steps; ++k) {
        const Vec2 p = on_circle(r, static_cast<double>(k) / steps);
        const int x = cx + static_cast<int>(std::lround(p.x));
        const int y = cy + static_cast<int>(std::lround(p.y));
        stairs.lines.push_back(static_cast<std::int32_t>(stairs.trace.primitives.size()));
        stairs.trace.primitives.push_back(traced_line(px, py, x, y));
        px = x;
        py = y;
    }
    return stairs;
}

// The arc of radius r around the centre of pixel (cx, cy), flattened.
std::vector<Vec2> arc(int cx, int cy, double r) {
    std::vector<Vec2> points;
    constexpr int chords = 360;
    points.reserve(chords + 1);
    for (int i = 0; i <= chords; ++i) {
        const Vec2 p = on_circle(r, static_cast<double>(i) / chords);
        points.push_back({.x = cx + 0.5 + p.x, .y = cy + 0.5 + p.y});
    }
    return points;
}

} // namespace

SCI_TEST(round_trip_of_a_straight_line_is_exact) {
    // Shallow, steep, and in every direction; drawn as one segment and as
    // the short chords of a flattened curve.
    const std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> lines = {
        {{10, 20}, {40, 31}}, {{40, 31}, {10, 20}}, {{5, 5}, {12, 60}},
        {{12, 60}, {5, 5}},   {{0, 0}, {319, 189}}, {{100, 50}, {130, 50}},
    };
    for (const auto& [a, b] : lines) {
        const auto [x0, y0] = a;
        const auto [x1, y1] = b;
        const auto expected = path1_pixels(x0, y0 + 10, x1, y1 + 10);
        std::vector<std::uint32_t> shifted;
        shifted.reserve(expected.size());
        for (const std::uint32_t p : expected) {
            shifted.push_back(p - (10 * 320));
        }
        SCI_CHECK(sci::render::rasterize_1x(std::vector{centre(x0, y0), centre(x1, y1)}) == shifted);

        sci::pic::DrawTrace trace;
        trace.primitives.push_back(traced_line(x0, y0, x1, y1));
        const auto result = sci::render::check_round_trip(
            trace, one_chain({0}, {{centre(x0, y0), centre(x1, y1)}}, sci::vector::FittedKind::Line));
        SCI_CHECK(result.chains.size() == 1);
        SCI_CHECK(result.passing == 1 && result.failing == 0);
        SCI_CHECK(result.chains[0].max_distance == 0);
        SCI_CHECK(result.chains[0].added == 0 && result.chains[0].missed == 0);
        SCI_CHECK(result.chains[0].mean_milli() == 0);

        // In short chords, ties may round the other way: within one pixel.
        const auto flattened = sci::render::check_round_trip(
            trace, one_chain({0}, {chords(centre(x0, y0), centre(x1, y1))}, sci::vector::FittedKind::Line));
        SCI_CHECK(flattened.chains[0].max_distance <= 1);
    }
}

SCI_TEST(round_trip_snaps_staircase_corners) {
    // A 2:1 line through pixel centres, as chords: the corners the chords
    // pass through are dropped, and path 1's pixels remain.
    const auto pixels = sci::render::rasterize_1x(chords(centre(0, 0), centre(4, 2)));
    SCI_CHECK(pixels.size() == 5);
    SCI_CHECK(pixels == (std::vector<std::uint32_t>{0, 321, 322, 643, 644}));
}

SCI_TEST(round_trip_accepts_a_right_arc_and_rejects_a_wrong_one) {
    const Staircase stairs = quarter_circle(100, 100, 30, 9);
    // The strokes of the chain: the arc, split among the lines as the
    // fitting would; here all on the first line, the others empty of fits.
    const auto check = [&](double radius) {
        std::vector<std::vector<Vec2>> strokes(stairs.lines.size(), std::vector<Vec2>{});
        strokes[0] = arc(100, 100, radius);
        auto stats = one_chain(stairs.lines, strokes, sci::vector::FittedKind::Arc);
        // Lines without a stroke of their own add nothing.
        stats.fitted_strokes.resize(1);
        return sci::render::check_round_trip(stairs.trace, stats);
    };
    const auto right = check(30);
    SCI_CHECK(right.passing == 1);
    SCI_CHECK(right.chains[0].max_distance <= 1);
    SCI_CHECK(right.chains[0].kind == sci::vector::FittedKind::Arc);

    const auto wrong = check(33);
    SCI_CHECK(wrong.failing == 1 && wrong.passing == 0);
    SCI_CHECK(wrong.chains[0].max_distance == 3);
    SCI_CHECK(wrong.chains[0].missed > 0 && wrong.chains[0].added > 0);
    SCI_CHECK(wrong.chains[0].worst_line >= 0);
}

SCI_TEST(round_trip_tolerance_is_one_pixel) {
    sci::pic::DrawTrace trace;
    trace.primitives.push_back(traced_line(10, 50, 40, 50));
    const auto offset = [&](int dy) {
        return sci::render::check_round_trip(
            trace, one_chain({0}, {{centre(10, 50 + dy), centre(40, 50 + dy)}}, sci::vector::FittedKind::Line));
    };
    const auto one = offset(1);
    SCI_CHECK(one.chains[0].max_distance == 1);
    SCI_CHECK(one.chains[0].passes());
    SCI_CHECK(one.chains[0].added == 31 && one.chains[0].missed == 31);
    SCI_CHECK(one.chains[0].distance_sum == 62);
    SCI_CHECK(one.chains[0].mean_milli() == 1000);

    const auto two = offset(-2);
    SCI_CHECK(two.chains[0].max_distance == 2);
    SCI_CHECK(!two.chains[0].passes());
    SCI_CHECK(two.failing == 1);
    // The first pixel in raster order with the largest distance: the
    // added row lies above the original.
    SCI_CHECK(two.chains[0].worst_x == 10 && two.chains[0].worst_y == 48);
    SCI_CHECK(two.chains[0].worst_line == 0);

    // One stray pixel two away from the end fails the whole chain.
    const auto spur = sci::render::check_round_trip(
        trace, one_chain({0}, {{centre(10, 50), centre(40, 50), centre(42, 50)}}, sci::vector::FittedKind::Line));
    SCI_CHECK(spur.chains[0].max_distance == 2);
    SCI_CHECK(spur.chains[0].added == 2 && spur.chains[0].missed == 0);
    SCI_CHECK(spur.chains[0].worst_x == 42 && spur.chains[0].worst_y == 50);
}
