// SPDX-License-Identifier: GPL-3.0-or-later
//
// The smoothing detector of milestone 10 (docs/hd-quality.md, "Smoothing
// suspects"): places where the HD reconstruction may have smoothed an
// intentionally sharp or regular shape, the defect class the customer found
// in pilot batch 1 (docs/ASSUMPTIONS.md, "the customer's answers to pilot
// batch 1"). It reads what the vector analysis exposes (the changes of the
// stroke integrity pass, the fitted chains and their strokes) against the
// 1x lines, and changes nothing. Three kinds:
//
// - Step: the stroke integrity pass joined or clipped the end of a
//   horizontal or vertical line onto a parallel line one to three pixels
//   away, with the ends offset along the lines: a connector across stacked
//   lines turns an intended step into a slope or splits it (stairs,
//   shelves, planks, brickwork). Nearby changes form one suspect.
// - Corner: an automatic arc, spline or ellipse runs through a vertex where
//   two of its lines, each at least two pixels long, meet at a turn of more
//   than about 25 degrees and the neighbouring vertices turn at most half
//   as much (the turn is concentrated in one pixel), and the fitted stroke
//   passes at least a quarter pixel from that vertex's pixel centre: an
//   intended corner rounded.
// - Curve: a run of PIC lines drawn end to end (consecutive commands) that
//   turns one way by at least about 70 degrees, whose vertices and 1x
//   pixels lie close to one ellipse (fitted by least squares), while the HD
//   strokes of those lines (fitted, or the lines themselves) leave that
//   ellipse by at least 0.3 pixels: a regular curve drawn lumpy or as a
//   polygon.
//
// Excluded: lines of organic things (A4 A.4: scene-model objects of an
// organic type, 3a chain hints and region hints whose label names trees,
// foliage, rocks, clouds, water, terrain or figures), and lines whose shape
// the scene model already steers (docs/hd-pipeline.md, section 6).
//
// Doubles for the geometry (+, -, *, / and sqrt only, spec section 5.3);
// the results are integers, sorted, so the output is deterministic.
#pragma once

#include "sci/pic/draw_trace.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci/vector/hints.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace sci::render {

enum class SmoothingKind : std::int8_t { Step, Corner, Curve };

struct SmoothingSuspect {
    SmoothingKind kind = SmoothingKind::Step;
    int score = 0;
    // A representative pixel (picture coordinates): the first change of a
    // step group, the corner's vertex, the curve's point furthest from the
    // ellipse.
    int x = 0;
    int y = 0;
    // The pixels concerned, inclusive (picture coordinates).
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::vector<std::int32_t> lines; // Trace indices, ascending.
    // Step: the changes in the group. Corner: 1. Curve: the run's lines.
    int count = 0;
    // Corner: 1 - cos of the turn at the vertex, per mille (90 degrees is
    // 1000). Curve: the run's total turn in degrees, approximately (exact
    // at multiples of 45). Step: the largest perpendicular offset of the
    // stacked lines in pixels.
    int bend = 0;
    // Corner: how far the HD stroke passes from the vertex's pixel centre.
    // Curve: the HD strokes' largest distance from the ellipse. Step: the
    // largest offset of the ends along the lines. Thousandths of a pixel
    // (step: pixels).
    int deviation = 0;
    // Curve: the largest distance of the run's 1x pixel centres from the
    // ellipse, thousandths of a pixel.
    int residual = 0;
};

// Whether a hint label names an organic thing (A4 A.4): trees, foliage,
// rocks, clouds, water, terrain or figures, by whole words.
[[nodiscard]] bool is_organic_label(std::string_view label);

// Per trace primitive: 1 if it is a line of an organic thing (see above).
// Region hints are applied to the 1x region of their pixel (same colour
// pair, 4-connected): a line counts if half of its pixels lie in or next
// to such a region. Hint coordinates are read as picture coordinates (no
// mirroring).
[[nodiscard]] std::vector<std::uint8_t> organic_lines(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                                      std::span<const vector::PictureHints* const> hints);

// Every suspect of a picture's visual scene, highest score first (then
// kind, y, x).
[[nodiscard]] std::vector<SmoothingSuspect> find_smoothing_suspects(
    const pic::PictureMaps& maps, const pic::DrawTrace& trace, const vector::SceneStats& visual,
    std::span<const vector::PictureHints* const> hints = {});

} // namespace sci::render
