// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shared boundary (spec section 6.6; docs/hd-pipeline.md, "The shared
// boundary"): a shape the scene model steers becomes the boundary of the
// faces on both sides of its lines, not only their strokes.
//
// The arrangement keeps its faces; what changes is where their edges run.
// Every edge of a steered line follows the line's piece, and every vertex
// on the line moves onto the piece (a line end to the piece's end; a vertex
// where one other straight line meets, along that line; otherwise to the
// nearest point). The other edges at a moved vertex end at its new point.
// A shape is taken only if the result is still a partition with the same
// faces: no moved edge crosses another edge, no vertex lies between a moved
// edge's old and new course, the order of the vertices along each line is
// kept, and no vertex is moved to two places. Otherwise nothing changes.
#pragma once

#include "curves.hpp"
#include "topology.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sci::vector::detail {

// One line of a steered shape: its segment's index (the arrangement's
// source), its end points (pixel centres, picture coordinates) and its
// piece, from `from` to `to`.
struct BoundaryLine {
    std::int32_t source = 0;
    Vec2 from;
    Vec2 to;
    std::vector<Vec2> piece;
};

class SharedBoundary {
public:
    // The 3b boundary: fitted pieces where the fit replaced edges, the
    // straight edges elsewhere.
    SharedBoundary(const Arrangement& arrangement, const std::vector<FittedChain>& fitted);

    // Makes the pieces the boundary of their lines' edges. Empty on
    // success; otherwise why not, and nothing has changed.
    [[nodiscard]] std::string apply(const std::vector<BoundaryLine>& lines);

    // Whether any shape was applied.
    [[nodiscard]] bool changed() const { return changed_; }

    // The course of every edge, from half-edge 2i's origin to its twin's;
    // nothing where it is the 3b course.
    [[nodiscard]] const std::vector<std::optional<std::vector<Vec2>>>& overrides() const { return overrides_; }

private:
    const Arrangement* arrangement_;
    std::vector<std::vector<Vec2>> base_;    // Per edge, the 3b course.
    std::vector<std::vector<Vec2>> course_;  // Per edge, the current course.
    std::vector<std::optional<Vec2>> moved_; // Per vertex, where steering put it.
    std::vector<std::optional<std::vector<Vec2>>> overrides_;
    bool changed_ = false;
};

} // namespace sci::vector::detail
