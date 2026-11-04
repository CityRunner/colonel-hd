// SPDX-License-Identifier: GPL-3.0-or-later
//
// The scene model steers the fit (A4 A.4, A.6, A.7; docs/hd-pipeline.md,
// section 6): a primitive of a decided or certain object says what its
// lines are meant to be, and their strokes follow that shape instead of
// the automatic fit.
//
// - `line`: one straight segment from the first line's start to the last
//   line's end.
// - `polyline`: straight segments meeting at its corners (sharp or step;
//   an inner point without a corner is a sharp corner too). A corner is
//   the end of one of its lines nearest the corner's point, within one
//   pixel, else the point's centre.
// - `ellipse` (open: first and last point differ): half of an ellipse, the
//   segment between the line ends being a diameter, fitted to the anchor
//   points and the lines' pixels (a circle seen in perspective).
// - `ellipse` (whole: first and last point equal): one ellipse fitted to
//   the anchor points and all the lines' pixels; each line's piece is the
//   part between its ends projected onto it. The lines need not form one
//   path (a gap stays open).
// - `arc`: a circular arc from the first line's start to the last line's
//   end, the circle fitted to the ends, the anchor points and the lines'
//   pixels, in the direction the first line leaves the start; it may turn
//   by more than half a circle (a hook).
//
// Otherwise the lines must form one path in the primitive's order (each one starts
// on or next to the pixel where the previous one ends, in either
// direction); the shape is then cut where they meet, one piece per line.
// It is used only if, rasterised back to 1x, the pieces lie within
// `round_trip_limit` of the lines' pixels (A.6); otherwise nothing changes.
//
// Separately, `step_cuts` squares the ends of horizontal and vertical lines
// at the step corners of a polyline, so that stacked strokes tile the pixel
// staircase exactly (horizontal treads, vertical risers, sharp corners).
#pragma once

#include "sci/pic/geometry.hpp"
#include "sci/vector/hints.hpp"
#include "sci/vector/scene.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sci::vector::detail {

// A4 A.6, as in sci/render/round_trip.hpp.
inline constexpr int round_trip_limit = 1;

struct SteerLine {
    pic::ScreenPoint from;
    pic::ScreenPoint to;
};

struct SteerInput {
    PrimitiveKind kind = PrimitiveKind::Line;
    std::vector<SteerLine> lines;                  // In the primitive's order.
    std::vector<Vec2> points;                      // The anchors' pixel centres, picture coordinates.
    std::vector<std::optional<CornerKind>> corner; // Per point.
};

struct Steered {
    bool applicable = false;               // The kind is steered and the lines form one path.
    std::vector<std::vector<Vec2>> pieces; // Per line, its stroke; empty unless used.
    int deviation = 0;                     // Round trip, in original pixels (when applicable).
    std::string reason;                    // Why it was not used, when applicable.
};

// With `solved` (the relation solver's anchors, relations.hpp), a line or
// polyline runs through those points instead of its lines' ends and
// corners, each line of a polyline between its two vertices; the checks are
// the same.
[[nodiscard]] Steered steer(const SteerInput& input, const std::vector<Vec2>* solved = nullptr);

// The points a line (its path's two ends) or a polyline (its vertices: the
// ends and corners) runs through, which the relation solver may move;
// nothing for other kinds, or if the lines form no path or a corner is
// rounded.
[[nodiscard]] std::optional<std::vector<Vec2>> anchors(const SteerInput& input);

struct StepCut {
    std::size_t line = 0; // Index into SteerInput::lines.
    Vec2 end;             // The end's pixel centre, where the stroke ends.
    Vec2 edge;            // Where it ends instead: the pixel's outer edge.
    Cut cut;              // Through the edge, square to the line.
};

// The square ends of a polyline's horizontal and vertical lines whose end
// pixel is one of its step corners: the stroke runs on to the pixel's
// outer edge and is cut there, so it covers the end pixel exactly.
[[nodiscard]] std::vector<StepCut> step_cuts(const SteerInput& input);

} // namespace sci::vector::detail
