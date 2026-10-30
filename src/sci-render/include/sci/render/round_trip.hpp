// SPDX-License-Identifier: GPL-3.0-or-later
//
// The round-trip check of A4 A.6 (docs/SPEC_ADDENDA.md): every fitted chain
// of a scene, rasterised back to 1x with path 1's line rule, against the
// original pixels of the PIC lines it replaces.
//
// - Reconstruction: the strokes of the chain's lines as fitted, before the
//   stroke integrity pass (SceneStats::fitted_strokes). Each polyline is
//   rasterised by `rasterize_1x`: every point is snapped to the pixel it
//   lies in, a pixel that only turns a corner between two pixels that touch
//   (a staircase step) is dropped, and consecutive pixels are joined by
//   path 1's line rule (sci::pic::for_each_line_pixel).
// - Original: the pixels the chain's lines drew by path 1's line rule,
//   within the picture area, whether or not later commands drew over them.
// - Distance: the Chebyshev distance between pixels (8-neighbours are 1
//   apart), in original pixels. A pixel of either set that is not in the
//   other has the distance to the nearest pixel of the other set; a pixel
//   in both has 0.
// - Deviation of a chain: the largest of these distances, and their mean
//   over all pixels of both sets (in thousandths of a pixel).
// - Verdict: a chain passes if its deviation is at most
//   `round_trip_tolerance`.
//
// Integers only: snapping is the only step that reads the curve's doubles.
#pragma once

#include "sci/pic/draw_trace.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci/vector/scene.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sci::render {

// A4 A.6: "no more than about 1 original pixel", as a Chebyshev distance.
inline constexpr int round_trip_tolerance = 1;

// The result for one fitted chain.
struct RoundTripChain {
    std::size_t chain = 0; // Index in SceneStats::fitted_chains.
    vector::FittedKind kind = vector::FittedKind::Line;
    bool hinted = false;
    std::int32_t first_line = -1; // Trace index of the chain's first line.
    int original_pixels = 0;
    int reconstructed_pixels = 0;
    int added = 0;       // Reconstructed, not original.
    int missed = 0;      // Original, not reconstructed.
    int missed_seen = 0; // Of the missed, pixels one of the chain's lines owns at 1x.
    int max_distance = 0;
    std::int64_t distance_sum = 0;
    // Where the deviation is largest (the first such pixel in raster
    // order), in picture coordinates, and the line it belongs to (trace
    // index): the line whose stroke added it, or the line that drew it.
    int worst_x = 0;
    int worst_y = 0;
    std::int32_t worst_line = -1;

    // Mean distance over both sets' pixels, in thousandths of a pixel,
    // rounded half up.
    [[nodiscard]] std::int64_t mean_milli() const;

    [[nodiscard]] bool passes() const { return max_distance <= round_trip_tolerance; }
};

struct RoundTrip {
    std::vector<RoundTripChain> chains;
    int passing = 0;
    int failing = 0;
};

// Rasterises a polyline in picture coordinates to 1x as described above:
// picture pixel indices (y * picture_width + x), ascending, without
// duplicates. Points outside the picture area are moved to its edge.
[[nodiscard]] std::vector<std::uint32_t> rasterize_1x(std::span<const vector::Vec2> points);

// Checks every fitted chain of a scene built from this trace.
[[nodiscard]] RoundTrip check_round_trip(const pic::DrawTrace& trace, const vector::SceneStats& stats);

} // namespace sci::render
