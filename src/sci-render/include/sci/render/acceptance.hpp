// SPDX-License-Identifier: GPL-3.0-or-later
//
// The acceptance check of the HD pipeline (spec section 6.4): the visual
// scene, reduced back to 1x, against path 1's pair map.
//
// - Pair classes: pairs are compared by the colour they blend to, so a pair
//   and its mirror image ((a, b) and (b, a)) are one class. Blended colours
//   are never compared with dithered pixels.
// - Agreement: the share of pixels outside the edge band (pixels with a
//   neighbour of another class, among their eight) whose reduced class is
//   the map's. The target is at least 99%.
// - Swaps: groups of four or more 4-connected differing pixels outside the
//   edge band, which would be a region shown in another colour rather than
//   an edge effect. The target is none.
// - Lines: every pixel a visible PIC line owns at 1x must be touched by the
//   line's stroke (at least one of its 16 x 16 samples lies within the
//   stroke). Pixels whose samples later layers cover completely are counted
//   separately; they show the drawing order, not a missing line.
//   Approved deviation (docs/ASSUMPTIONS.md, "Milestone 10: the customer's
//   decisions on the halo and the curtain rings"): a pixel of a line the
//   caller names as decided by the customer counts as tolerated, not
//   untouched, if the stroke passes within 1 original pixel of it in the
//   sense of A4 A.6 (vector::within_round_trip_tolerance); every such
//   pixel is listed.
#pragma once

#include "sci/pic/draw_trace.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci/vector/scene.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sci::render {

// A line pixel taken within the A.6 tolerance.
struct ToleratedPixel {
    std::int32_t primitive = 0; // The line's index in the trace.
    int x = 0;                  // Picture coordinates.
    int y = 0;
};

struct Acceptance {
    int compared_pixels = 0;  // Outside the edge band.
    int differing_pixels = 0; // Of those, with another class after reduction.
    int swaps = 0;
    int line_pixels = 0;                               // Pixels owned by visible lines.
    int untouched_line_pixels = 0;                     // Not touched by their line's stroke.
    int overlapped_line_pixels = 0;                    // Touched, but covered by later layers.
    std::vector<ToleratedPixel> tolerated_line_pixels; // Not touched, within the tolerance (decided lines).

    [[nodiscard]] double agreement() const {
        return compared_pixels == 0 ? 1.0 : 1.0 - (static_cast<double>(differing_pixels) / compared_pixels);
    }

    [[nodiscard]] bool passes() const { return agreement() >= 0.99 && swaps == 0 && untouched_line_pixels == 0; }
};

// Checks a visual scene built from these maps and trace. `decided` holds
// the trace indices of the lines whose pixels are taken within the A.6
// tolerance (vector::SceneStats::decided_lines); `widened` those of them
// whose shape has a wider tolerance the customer approved, taken within 2
// pixels (vector::SceneStats::widened_lines).
[[nodiscard]] Acceptance check_acceptance(const vector::Scene& visual, const pic::PictureMaps& maps,
                                          const pic::DrawTrace& trace, std::span<const std::int32_t> decided = {},
                                          std::span<const std::int32_t> widened = {});

} // namespace sci::render
