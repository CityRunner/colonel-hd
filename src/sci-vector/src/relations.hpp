// SPDX-License-Identifier: GPL-3.0-or-later
//
// Relation solving (A4 A.3; docs/hd-pipeline.md, section 6, "Relation
// solving"): the geometric relations of one decided or certain object are
// made to hold exactly, jointly for the whole object, before its shapes are
// steered. The unknowns are the anchor points of its primitives (a line's
// two path ends, a polyline's vertices); they move as little as possible
// (least squares on the points' displacements) so that the relations hold.
//
// - `symmetry` (horizontal or vertical axis): the items' points are mirror
//   images of each other in pairs, or lie on the axis; the axis' position
//   is part of the solution (the hint files store none).
// - `parallel`, `perpendicular`: of line items (their directions).
// - `aligned` (axis): the items' centres share one x (vertical) or one y
//   (horizontal).
// - `equal-spacing`: consecutive items' centres are equally far apart
//   across the first item's first line.
// - Points tied together (one corner shared by two primitives) stay one
//   point.
//
// The constraints are linearised and solved with the minimum-norm step,
// x = x0 + J^T lambda, (J J^T) lambda = -g(x) - J (x0 - x), a fixed number
// of times at most (deterministic; only +, -, *, / and sqrt). Dependent
// constraints (two symmetries of a rhombus state some facts twice) are left
// out by Gram-Schmidt; afterwards every constraint must hold. A solution
// that moves a point by more than `max_move` original pixels (Chebyshev,
// the A.6 measure) is rejected.
#pragma once

#include "sci/vector/hints.hpp"
#include "sci/vector/scene.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sci::vector::detail {

// A4 A.6: at most about 1 original pixel.
inline constexpr double max_move = 1.0;

// One point of one primitive.
struct AnchorRef {
    std::size_t primitive = 0;
    std::size_t point = 0;

    friend bool operator==(const AnchorRef&, const AnchorRef&) = default;
};

struct SolvePrimitive {
    PrimitiveKind kind = PrimitiveKind::Line; // Line or Polyline.
    std::vector<Vec2> anchors;                // Line: its two ends; polyline: its vertices.
};

// A relation over items; an item is one or more primitives (a part).
struct SolveRelation {
    GeometricKind kind = GeometricKind::Parallel;
    Axis axis = Axis::None;
    std::vector<std::vector<std::size_t>> items;
};

struct RelationProblem {
    std::vector<SolvePrimitive> primitives;
    std::vector<SolveRelation> relations;
    std::vector<std::pair<AnchorRef, AnchorRef>> ties; // Points that are one point.
};

struct RelationSolution {
    bool solved = false;
    std::vector<std::vector<Vec2>> anchors; // Per primitive, when solved.
    double largest_move = 0;                // Chebyshev, original pixels.
    std::string reason;                     // Why not, when not solved.
};

[[nodiscard]] RelationSolution solve_relations(const RelationProblem& problem);

} // namespace sci::vector::detail
