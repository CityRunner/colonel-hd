// SPDX-License-Identifier: GPL-3.0-or-later

#include "relations.hpp"
#include "scene_model.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci_test.hpp"

#include <cmath>
#include <vector>

using sci::pic::picture_top;
using sci::pic::ScreenPoint;
using sci::vector::Axis;
using sci::vector::CornerKind;
using sci::vector::GeometricKind;
using sci::vector::PrimitiveKind;
using sci::vector::Vec2;
using sci::vector::detail::AnchorRef;
using sci::vector::detail::RelationProblem;
using sci::vector::detail::SolvePrimitive;
using sci::vector::detail::SolveRelation;
using sci::vector::detail::SteerInput;
using sci::vector::detail::SteerLine;

namespace {

bool near(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

SolvePrimitive line(double x0, double y0, double x1, double y1) {
    return {.kind = PrimitiveKind::Line, .anchors = {{.x = x0, .y = y0}, {.x = x1, .y = y1}}};
}

// Picture 42's rhombus (A3.2) as drawn: the dark upper half from the left
// corner over the top to the right corner, the light lower half back; the
// halves' left and right corners are one point each.
RelationProblem lopsided_rhombus() {
    RelationProblem problem;
    problem.primitives = {
        {.kind = PrimitiveKind::Polyline,
         .anchors = {{.x = 181.5, .y = 27.5}, {.x = 190.5, .y = 9.5}, {.x = 201.5, .y = 26.5}}},
        {.kind = PrimitiveKind::Polyline,
         .anchors = {{.x = 201.5, .y = 28.5}, {.x = 191.5, .y = 45.5}, {.x = 180.5, .y = 27.5}}},
    };
    problem.ties = {{AnchorRef{.primitive = 0, .point = 0}, AnchorRef{.primitive = 1, .point = 2}},
                    {AnchorRef{.primitive = 0, .point = 2}, AnchorRef{.primitive = 1, .point = 0}}};
    problem.relations = {
        {.kind = GeometricKind::Symmetry, .axis = Axis::Vertical, .items = {{0}, {1}}},
        {.kind = GeometricKind::Symmetry, .axis = Axis::Horizontal, .items = {{0}, {1}}},
    };
    return problem;
}

} // namespace

// A lopsided diamond becomes symmetric about both of its diagonals, its
// shared corners one point each, every point moved by at most a pixel.
SCI_TEST(lopsided_diamond_becomes_symmetric_about_both_axes) {
    const auto solution = sci::vector::detail::solve_relations(lopsided_rhombus());
    SCI_CHECK(solution.solved);
    if (!(solution.solved)) {
        return;
    }
    const Vec2 left = solution.anchors[0][0];
    const Vec2 top = solution.anchors[0][1];
    const Vec2 right = solution.anchors[0][2];
    const Vec2 bottom = solution.anchors[1][1];
    SCI_CHECK(near(left.x, solution.anchors[1][2].x) && near(left.y, solution.anchors[1][2].y));
    SCI_CHECK(near(right.x, solution.anchors[1][0].x) && near(right.y, solution.anchors[1][0].y));
    SCI_CHECK(near(top.x, bottom.x));                // The vertical diagonal.
    SCI_CHECK(near(left.y, right.y));                // The horizontal diagonal.
    SCI_CHECK(near((left.x + right.x) / 2, top.x));  // Mirrored about the vertical one.
    SCI_CHECK(near((top.y + bottom.y) / 2, left.y)); // Mirrored about the horizontal one.
    SCI_CHECK(solution.largest_move <= 1.0 + 1e-9);
    SCI_CHECK(solution.largest_move > 0.5);
}

// The solved rhombus, steered: straight sides through the solved corners,
// within the round trip of the drawn lines.
SCI_TEST(solved_polyline_is_steered_through_its_solved_corners) {
    const auto solution = sci::vector::detail::solve_relations(lopsided_rhombus());
    SCI_CHECK(solution.solved);
    if (!(solution.solved)) {
        return;
    }
    const auto at = [](int x, int y) { return ScreenPoint{.x = x, .y = y + picture_top}; };
    const SteerInput dark{
        .kind = PrimitiveKind::Polyline,
        .lines = {SteerLine{.from = at(189, 10), .to = at(181, 27)}, SteerLine{.from = at(190, 9), .to = at(201, 26)}},
        .points = {{.x = 180.5, .y = 27.5}, {.x = 190.5, .y = 9.5}, {.x = 201.5, .y = 27.5}},
        .corner = {std::nullopt, CornerKind::Sharp, std::nullopt},
    };
    const auto anchors = sci::vector::detail::anchors(dark);
    SCI_CHECK(anchors.has_value() && anchors->size() == 3);
    if (!(anchors.has_value() && anchors->size() == 3)) {
        return;
    }
    SCI_CHECK(near((*anchors)[1].x, 190.5) && near((*anchors)[1].y, 9.5));
    const auto steered = sci::vector::detail::steer(dark, &solution.anchors[0]);
    SCI_CHECK(steered.applicable && steered.pieces.size() == 2);
    if (!(steered.applicable && steered.pieces.size() == 2)) {
        return;
    }
    SCI_CHECK(steered.deviation <= 1);
    // The first line runs against the path: its piece ends at the corner.
    SCI_CHECK(steered.pieces[0].front() == solution.anchors[0][1] ||
              steered.pieces[0].back() == solution.anchors[0][1]);
    SCI_CHECK(steered.pieces[1].front() == solution.anchors[0][1]);
}

// Two nearly parallel lines become parallel; a third at a right angle to
// the first becomes exactly perpendicular.
SCI_TEST(parallel_and_perpendicular_hold_after_solving) {
    RelationProblem problem;
    problem.primitives = {line(0.5, 0.5, 30.5, 0.5), line(0.5, 10.5, 30.5, 11.5), line(40.5, 0.5, 40.0, 20.5)};
    problem.relations = {
        {.kind = GeometricKind::Parallel, .axis = Axis::None, .items = {{0}, {1}}},
        {.kind = GeometricKind::Perpendicular, .axis = Axis::None, .items = {{0}, {2}}},
    };
    const auto solution = sci::vector::detail::solve_relations(problem);
    SCI_CHECK(solution.solved);
    if (!(solution.solved)) {
        return;
    }
    const auto direction = [&](std::size_t p) {
        return Vec2{.x = solution.anchors[p][1].x - solution.anchors[p][0].x,
                    .y = solution.anchors[p][1].y - solution.anchors[p][0].y};
    };
    const Vec2 a = direction(0);
    const Vec2 b = direction(1);
    const Vec2 c = direction(2);
    SCI_CHECK(std::abs((a.x * b.y) - (a.y * b.x)) < 1e-6);
    SCI_CHECK(std::abs((a.x * c.x) + (a.y * c.y)) < 1e-6);
    SCI_CHECK(solution.largest_move > 0 && solution.largest_move <= 1.0);
}

// Equal spacing of three lines a pixel off: the middle one moves halfway.
SCI_TEST(equal_spacing_moves_the_items_least) {
    RelationProblem problem;
    problem.primitives = {line(0.5, 0.5, 0.5, 20.5), line(5.5, 0.5, 5.5, 20.5), line(11.5, 0.5, 11.5, 20.5)};
    problem.relations = {{.kind = GeometricKind::EqualSpacing, .axis = Axis::None, .items = {{0}, {1}, {2}}}};
    const auto solution = sci::vector::detail::solve_relations(problem);
    SCI_CHECK(solution.solved);
    if (!(solution.solved)) {
        return;
    }
    const double first = solution.anchors[1][0].x - solution.anchors[0][0].x;
    const double second = solution.anchors[2][0].x - solution.anchors[1][0].x;
    SCI_CHECK(near(first, second));
    SCI_CHECK(solution.largest_move < 0.5);
}

// A relation the drawing is far from (two lines 10 degrees apart called
// parallel) would move points by more than a pixel: rejected, with the
// reason, and nothing is returned.
SCI_TEST(relation_needing_more_than_a_pixel_is_rejected) {
    RelationProblem problem;
    problem.primitives = {line(0.5, 0.5, 30.5, 0.5), line(0.5, 10.5, 30.5, 15.5)};
    problem.relations = {{.kind = GeometricKind::Parallel, .axis = Axis::None, .items = {{0}, {1}}}};
    const auto solution = sci::vector::detail::solve_relations(problem);
    SCI_CHECK(!solution.solved);
    SCI_CHECK(solution.anchors.empty());
    SCI_CHECK(solution.reason.starts_with("the relations need a move of"));
}

// Symmetry without a mirror partner for a point is not solved.
SCI_TEST(symmetry_without_partners_is_rejected) {
    RelationProblem problem;
    problem.primitives = {line(0.5, 0.5, 10.5, 0.5), line(20.5, 5.5, 22.5, 30.5)};
    problem.relations = {{.kind = GeometricKind::Symmetry, .axis = Axis::Vertical, .items = {{0}, {1}}}};
    const auto solution = sci::vector::detail::solve_relations(problem);
    SCI_CHECK(!solution.solved);
}
