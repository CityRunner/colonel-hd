// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rasterising a polyline back to 1x with path 1's line rule, as the
// round-trip check of A4 A.6 does (sci/render/round_trip.hpp describes the
// rule). It lives here so that the fits the scene model steers can be
// checked while the scene is built (docs/hd-pipeline.md, section 6).
#pragma once

#include "sci/pic/geometry.hpp"
#include "sci/vector/scene.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sci::vector {

// Picture pixel indices (y * picture_width + x), ascending, without
// duplicates: every point snapped to the pixel it lies in (points outside
// the picture area to its edge), a pixel that only turns a staircase corner
// dropped, consecutive pixels joined by path 1's line rule.
[[nodiscard]] std::vector<std::uint32_t> rasterize_1x(std::span<const Vec2> points);

// The pixels a PIC line from `from` to `to` (screen coordinates) drew within
// the picture area, ascending.
[[nodiscard]] std::vector<std::uint32_t> line_pixels_1x(pic::ScreenPoint from, pic::ScreenPoint to);

// The largest Chebyshev distance from a pixel of either set (ascending
// indices) to the nearest pixel of the other: 0 for equal sets, and
// picture_width if exactly one set is empty.
[[nodiscard]] int raster_deviation(std::span<const std::uint32_t> a, std::span<const std::uint32_t> b);

} // namespace sci::vector
