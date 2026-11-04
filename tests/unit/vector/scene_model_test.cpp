// SPDX-License-Identifier: GPL-3.0-or-later

#include "fitting.hpp"
#include "scene_model.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci/vector/raster_1x.hpp"
#include "sci_test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

using sci::pic::picture_top;
using sci::pic::ScreenPoint;
using sci::vector::CornerKind;
using sci::vector::Cut;
using sci::vector::PrimitiveKind;
using sci::vector::Vec2;
using sci::vector::detail::polyline_distance;
using sci::vector::detail::SteerInput;
using sci::vector::detail::SteerLine;

namespace {

// A line between picture coordinates.
SteerLine line(int x0, int y0, int x1, int y1) {
    return {.from = ScreenPoint{.x = x0, .y = y0 + picture_top}, .to = ScreenPoint{.x = x1, .y = y1 + picture_top}};
}

Vec2 centre(int x, int y) {
    return {.x = x + 0.5, .y = y + 0.5};
}

double distance_to_segment(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 d{.x = b.x - a.x, .y = b.y - a.y};
    const double length_squared = (d.x * d.x) + (d.y * d.y);
    double t = length_squared == 0 ? 0 : (((p.x - a.x) * d.x) + ((p.y - a.y) * d.y)) / length_squared;
    t = std::clamp(t, 0.0, 1.0);
    const double dx = a.x + (d.x * t) - p.x;
    const double dy = a.y + (d.y * t) - p.y;
    return std::sqrt((dx * dx) + (dy * dy));
}

} // namespace

// Three stacked treads whose left ends step right by one pixel (the lit
// steps of picture 74): with their ends squared at the step corners, the
// strokes cover exactly the pixels the lines drew there, a staircase with
// horizontal treads, vertical risers and sharp corners.
SCI_TEST(step_corners_reproduce_a_pixel_staircase_exactly) {
    const SteerInput input{
        .kind = PrimitiveKind::Polyline,
        .lines = {line(64, 151, 25, 151), line(28, 152, 64, 152), line(29, 153, 64, 153)},
        .points = {centre(25, 151), centre(28, 152), centre(29, 153)},
        .corner = {CornerKind::Step, CornerKind::Step, CornerKind::Step},
    };
    const auto cuts = sci::vector::detail::step_cuts(input);
    SCI_CHECK(cuts.size() == 3);

    // Each stroke as build_scene makes it: run on to the pixel's edge, cut there.
    struct Stroke {
        Vec2 a;
        Vec2 b;
        std::vector<Cut> cuts;
    };

    std::vector<Stroke> strokes;
    for (std::size_t i = 0; i < input.lines.size(); ++i) {
        const SteerLine& l = input.lines[i];
        Stroke s{.a = centre(l.from.x, l.from.y - picture_top), .b = centre(l.to.x, l.to.y - picture_top), .cuts = {}};
        for (const auto& cut : cuts) {
            if (cut.line != i) {
                continue;
            }
            SCI_CHECK(cut.end == s.a || cut.end == s.b);
            (cut.end == s.a ? s.a : s.b) = cut.edge;
            s.cuts.push_back(cut.cut);
        }
        strokes.push_back(s);
    }
    // Sample 8 x 8 points per pixel over the left ends (x 20-40).
    for (int y = 149; y < 156; ++y) {
        for (int x = 20; x < 40; ++x) {
            const bool drawn = (y == 151 && x >= 25) || (y == 152 && x >= 28) || (y == 153 && x >= 29);
            for (int sy = 0; sy < 8; ++sy) {
                for (int sx = 0; sx < 8; ++sx) {
                    const Vec2 p{.x = x + ((sx + 0.5) / 8), .y = y + ((sy + 0.5) / 8)};
                    const bool covered = std::ranges::any_of(strokes, [&](const Stroke& s) {
                        return distance_to_segment(p, s.a, s.b) <= 0.5 &&
                               std::ranges::all_of(s.cuts, [&](const Cut& c) { return c.keeps(p); });
                    });
                    SCI_CHECK(covered == drawn);
                }
            }
        }
    }
}

// Ends that are not step corners, and lines that are not horizontal or
// vertical, keep their round ends.
SCI_TEST(only_axis_lines_at_step_corners_are_squared) {
    const SteerInput input{
        .kind = PrimitiveKind::Polyline,
        .lines = {line(10, 10, 20, 10), line(10, 11, 20, 14)},
        .points = {centre(20, 10), centre(10, 11)},
        .corner = {CornerKind::Step, CornerKind::Step},
    };
    const auto cuts = sci::vector::detail::step_cuts(input);
    SCI_CHECK(cuts.size() == 1);
    SCI_CHECK(cuts.front().line == 0 && cuts.front().edge == Vec2{.x = 21, .y = 10.5});
}

// A fanlight drawn as short PIC lines (a half ellipse 40 x 12 pixels,
// stepped to whole pixels) becomes half of an ellipse through both ends,
// within the tolerance, cut where the lines meet.
SCI_TEST(half_ellipse_fits_within_the_tolerance) {
    constexpr std::array<Vec2, 9> half{
        Vec2{.x = -1, .y = 0},
        Vec2{.x = -0.92388, .y = 0.38268},
        Vec2{.x = -0.70711, .y = 0.70711},
        Vec2{.x = -0.38268, .y = 0.92388},
        Vec2{.x = 0, .y = 1},
        Vec2{.x = 0.38268, .y = 0.92388},
        Vec2{.x = 0.70711, .y = 0.70711},
        Vec2{.x = 0.92388, .y = 0.38268},
        Vec2{.x = 1, .y = 0},
    };
    std::vector<std::array<int, 2>> knots;
    knots.reserve(half.size());
    for (const Vec2 u : half) {
        knots.push_back(
            {static_cast<int>(std::lround(100 + (20 * u.x))), static_cast<int>(std::lround(60 - (12 * u.y)))});
    }
    SteerInput input{.kind = PrimitiveKind::Ellipse, .lines = {}, .points = {}, .corner = {}};
    std::vector<Vec2> drawn;
    for (std::size_t i = 0; i + 1 < knots.size(); ++i) {
        input.lines.push_back(line(knots[i][0], knots[i][1], knots[i + 1][0], knots[i + 1][1]));
        drawn.push_back(centre(knots[i][0], knots[i][1]));
    }
    drawn.push_back(centre(knots.back()[0], knots.back()[1]));
    input.points = {drawn.front(), centre(100, 48), drawn.back()};
    input.corner = {std::nullopt, std::nullopt, std::nullopt};
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.pieces.size() == input.lines.size());
    SCI_CHECK(steered.deviation <= sci::vector::detail::round_trip_limit);
    std::vector<Vec2> curve;
    for (const auto& piece : steered.pieces) {
        SCI_CHECK(piece.size() >= 2);
        if (!curve.empty()) {
            SCI_CHECK(curve.back() == piece.front()); // The pieces join.
        }
        curve.insert(curve.end(), piece.begin() + (curve.empty() ? 0 : 1), piece.end());
    }
    SCI_CHECK(curve.front() == drawn.front() && curve.back() == drawn.back());
    SCI_CHECK(polyline_distance(curve, drawn) <= 1.0);
    // Smooth: no chord turns by more than 30 degrees.
    for (std::size_t i = 1; i + 1 < curve.size(); ++i) {
        const Vec2 d1{.x = curve[i].x - curve[i - 1].x, .y = curve[i].y - curve[i - 1].y};
        const Vec2 d2{.x = curve[i + 1].x - curve[i].x, .y = curve[i + 1].y - curve[i].y};
        const double dot = (d1.x * d2.x) + (d1.y * d2.y);
        const double lengths = std::sqrt(((d1.x * d1.x) + (d1.y * d1.y)) * ((d2.x * d2.x) + (d2.y * d2.y)));
        SCI_CHECK(lengths == 0 || dot >= 0.866 * lengths);
    }
}

namespace {

// The pieces of a steered primitive joined into one curve; empty if they
// do not join.
std::vector<Vec2> joined(const sci::vector::detail::Steered& steered) {
    std::vector<Vec2> curve;
    for (const auto& piece : steered.pieces) {
        if (piece.size() < 2 || (!curve.empty() && curve.back() != piece.front())) {
            return {};
        }
        curve.insert(curve.end(), piece.begin() + (curve.empty() ? 0 : 1), piece.end());
    }
    return curve;
}

// No chord turns by more than 30 degrees.
bool smooth(const std::vector<Vec2>& curve) {
    for (std::size_t i = 1; i + 1 < curve.size(); ++i) {
        const Vec2 d1{.x = curve[i].x - curve[i - 1].x, .y = curve[i].y - curve[i - 1].y};
        const Vec2 d2{.x = curve[i + 1].x - curve[i].x, .y = curve[i + 1].y - curve[i].y};
        const double dot = (d1.x * d2.x) + (d1.y * d2.y);
        const double lengths = std::sqrt(((d1.x * d1.x) + (d1.y * d1.y)) * ((d2.x * d2.x) + (d2.y * d2.y)));
        if (lengths != 0 && dot < 0.866 * lengths) {
            return false;
        }
    }
    return true;
}

// Short PIC lines through the knots, rounded to whole pixels, centred at
// (cx, cy) with the given radius, as an arc primitive anchored at its ends
// and at the knot `anchor`.
SteerInput stepped_arc(std::span<const Vec2> unit, int cx, int cy, int radius, std::size_t anchor,
                       std::vector<Vec2>& drawn) {
    std::vector<std::array<int, 2>> knots;
    for (const Vec2 u : unit) {
        knots.push_back(
            {static_cast<int>(std::lround(cx + (radius * u.x))), static_cast<int>(std::lround(cy + (radius * u.y)))});
    }
    SteerInput input{.kind = PrimitiveKind::Arc, .lines = {}, .points = {}, .corner = {}};
    drawn.clear();
    for (std::size_t i = 0; i + 1 < knots.size(); ++i) {
        input.lines.push_back(line(knots[i][0], knots[i][1], knots[i + 1][0], knots[i + 1][1]));
        drawn.push_back(centre(knots[i][0], knots[i][1]));
    }
    drawn.push_back(centre(knots.back()[0], knots.back()[1]));
    input.points = {drawn.front(), drawn[anchor], drawn.back()};
    input.corner = {std::nullopt, std::nullopt, std::nullopt};
    return input;
}

} // namespace

// A hook drawn as short PIC lines around three quarters of a circle of
// radius 12 (more than half a turn, which no half ellipse describes)
// becomes one circular arc through both ends, within the tolerance.
SCI_TEST(hook_of_three_quarter_turns_fits_as_a_circular_arc) {
    constexpr std::array<Vec2, 10> unit{
        Vec2{.x = 1, .y = 0},  Vec2{.x = 0.86603, .y = -0.5},  Vec2{.x = 0.5, .y = -0.86603},
        Vec2{.x = 0, .y = -1}, Vec2{.x = -0.5, .y = -0.86603}, Vec2{.x = -0.86603, .y = -0.5},
        Vec2{.x = -1, .y = 0}, Vec2{.x = -0.86603, .y = 0.5},  Vec2{.x = -0.5, .y = 0.86603},
        Vec2{.x = 0, .y = 1},
    };
    std::vector<Vec2> drawn;
    const SteerInput input = stepped_arc(unit, 100, 60, 12, 4, drawn);
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.pieces.size() == input.lines.size());
    SCI_CHECK(steered.deviation <= sci::vector::detail::round_trip_limit);
    const std::vector<Vec2> curve = joined(steered);
    SCI_CHECK(!curve.empty() && curve.front() == drawn.front() && curve.back() == drawn.back());
    SCI_CHECK(polyline_distance(curve, drawn) <= 1.0);
    SCI_CHECK(smooth(curve));
    // It goes the long way round, over the top (y < 60) and down the left.
    SCI_CHECK(std::ranges::any_of(curve, [](Vec2 p) { return p.y < 50; }));
    SCI_CHECK(std::ranges::any_of(curve, [](Vec2 p) { return p.x < 90; }));
}

// The same arc in the other direction, and a quarter of a circle: the
// direction comes from the first line.
SCI_TEST(arc_follows_the_first_line_from_the_start) {
    constexpr std::array<Vec2, 4> unit{
        Vec2{.x = 0, .y = -1},
        Vec2{.x = 0.5, .y = -0.86603},
        Vec2{.x = 0.86603, .y = -0.5},
        Vec2{.x = 1, .y = 0},
    };
    std::vector<Vec2> drawn;
    const SteerInput input = stepped_arc(unit, 50, 80, 16, 2, drawn);
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.deviation <= sci::vector::detail::round_trip_limit);
    const std::vector<Vec2> curve = joined(steered);
    SCI_CHECK(!curve.empty() && smooth(curve));
    SCI_CHECK(polyline_distance(curve, drawn) <= 1.0);
    // A quarter turn, not the three quarters the other way round.
    SCI_CHECK(std::ranges::all_of(curve, [](Vec2 p) { return p.x >= 49 && p.y <= 81; }));
}

// Lines with a sharp corner are no arc: the round trip rejects it and the
// 3b reconstruction stays.
SCI_TEST(an_arc_the_pixels_contradict_is_not_used) {
    const SteerInput input{
        .kind = PrimitiveKind::Arc,
        .lines = {line(10, 50, 30, 50), line(30, 50, 30, 70)},
        .points = {centre(10, 50), centre(30, 50), centre(30, 70)},
        .corner = {std::nullopt, std::nullopt, std::nullopt},
    };
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.pieces.empty());
    SCI_CHECK(steered.reason.contains("round trip"));
}

// A glint drawn as a vertical line, then two short horizontal ones a pixel
// apart (picture 58): a polyline with a sharp corner, straight on both
// sides, through the drawn corner.
SCI_TEST(sharp_polyline_is_straight_between_its_corners) {
    const SteerInput input{
        .kind = PrimitiveKind::Polyline,
        .lines = {line(233, 42, 233, 33), line(233, 43, 237, 43), line(238, 44, 239, 44)},
        .points = {centre(233, 33), centre(233, 43), centre(239, 44)},
        .corner = {std::nullopt, CornerKind::Sharp, std::nullopt},
    };
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.pieces.size() == 3 && steered.deviation <= 1);
    // The first piece follows its line's direction (from (233, 42) up).
    SCI_CHECK(steered.pieces[0].back() == centre(233, 33));
    const Vec2 corner = centre(233, 43);
    const Vec2 end = centre(239, 44);
    for (std::size_t i = 1; i < 3; ++i) {
        for (const Vec2 p : steered.pieces[i]) {
            SCI_CHECK(distance_to_segment(p, corner, end) < 1e-9);
        }
    }
    SCI_CHECK(steered.pieces[1].front() == corner && steered.pieces[2].back() == end);
}

// A hypothesis the pixels contradict fails the round trip and changes
// nothing (A4 A.6): the 3b reconstruction stays.
SCI_TEST(a_fit_that_fails_the_round_trip_is_not_used) {
    const SteerInput input{
        .kind = PrimitiveKind::Polyline,
        .lines = {line(10, 50, 20, 50), line(21, 50, 30, 50)},
        .points = {centre(10, 50), centre(20, 45), centre(30, 50)},
        .corner = {std::nullopt, CornerKind::Sharp, std::nullopt},
    };
    const auto steered = sci::vector::detail::steer(input);
    SCI_CHECK(steered.applicable && steered.pieces.empty());
    SCI_CHECK(steered.deviation > sci::vector::detail::round_trip_limit);
    SCI_CHECK(steered.reason.contains("round trip"));
}

// Lines side by side (a bar drawn two lines wide) are no path.
SCI_TEST(lines_side_by_side_are_not_steered) {
    const SteerInput input{
        .kind = PrimitiveKind::Line,
        .lines = {line(34, 49, 34, 43), line(33, 50, 33, 43)},
        .points = {centre(34, 49), centre(34, 43)},
        .corner = {std::nullopt, std::nullopt},
    };
    SCI_CHECK(!sci::vector::detail::steer(input).applicable);
}

SCI_TEST(round_trip_raster_of_a_straight_stroke_is_path_1s_line) {
    const std::array<Vec2, 2> stroke{centre(3, 4), centre(17, 9)};
    const auto back = sci::vector::rasterize_1x(stroke);
    const auto drawn = sci::vector::line_pixels_1x(ScreenPoint{.x = 3, .y = 4 + picture_top},
                                                   ScreenPoint{.x = 17, .y = 9 + picture_top});
    SCI_CHECK(back == drawn);
    SCI_CHECK(sci::vector::raster_deviation(back, drawn) == 0);
}
