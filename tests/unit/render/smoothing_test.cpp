// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/pic/draw_trace.hpp"
#include "sci/pic/geometry.hpp"
#include "sci/pic/picture_maps.hpp"
#include "sci/render/smoothing.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci/vector/hints.hpp"
#include "sci/vector/scene.hpp"
#include "sci_test.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using sci::render::SmoothingKind;
using sci::vector::Vec2;

namespace {

// A trace of visual lines, each owning the pixels it draws in its colour
// pair (later lines on top) on a background of pair 0, as path 1 would
// leave them.
struct Synthetic {
    sci::pic::PictureMaps maps;
    sci::pic::DrawTrace trace;
    sci::vector::SceneStats stats;

    std::int32_t line(int x0, int y0, int x1, int y1, sci::pic::ColorPair pair = 0x66) {
        sci::pic::TracedPrimitive primitive;
        primitive.command = static_cast<std::uint32_t>(trace.primitives.size());
        primitive.state.planes = sci::pic::plane_visual;
        primitive.state.pair = pair;
        primitive.shape = sci::pic::Line{
            .from = {.x = static_cast<std::int16_t>(x0), .y = static_cast<std::int16_t>(y0)},
            .to = {.x = static_cast<std::int16_t>(x1), .y = static_cast<std::int16_t>(y1)},
        };
        const auto index = static_cast<std::int32_t>(trace.primitives.size());
        trace.primitives.push_back(primitive);
        sci::pic::for_each_line_pixel({.x = x0, .y = y0 + sci::pic::picture_top},
                                      {.x = x1, .y = y1 + sci::pic::picture_top},
                                      [&](int x, int y) {
                                          trace.visual_owner[sci::pic::PictureMaps::index(x, y)] = index;
                                          maps.pairs[sci::pic::PictureMaps::index(x, y)] = pair;
                                      });
        return index;
    }

    [[nodiscard]] std::vector<sci::render::SmoothingSuspect>
    suspects(std::span<const sci::vector::PictureHints* const> hints = {}) const {
        return sci::render::find_smoothing_suspects(maps, trace, stats, hints);
    }
};

std::size_t count_kind(const std::vector<sci::render::SmoothingSuspect>& suspects, SmoothingKind kind) {
    std::size_t n = 0;
    for (const auto& s : suspects) {
        n += s.kind == kind ? 1 : 0;
    }
    return n;
}

sci::vector::IntegrityChange joint(std::int32_t a, std::int32_t b, int x, int y) {
    return {.kind = sci::vector::IntegrityChangeKind::ClosedJoint,
            .primitive = a,
            .other = b,
            .x = x,
            .y = y,
            .evidence = "test"};
}

Vec2 centre(int x, int y) {
    return {.x = x + 0.5, .y = y + 0.5};
}

} // namespace

// Kind 1: two stacked treads whose left ends touch diagonally, joined by
// the stroke integrity pass over pixels of another colour, are a step
// suspect; a joint of flush ends (a bar drawn two lines wide) is not, nor
// a joint over pixels of the line's own colour.
SCI_TEST(smoothing_step_joint_between_stacked_lines) {
    Synthetic s;
    const auto upper = s.line(20, 40, 40, 40);
    const auto lower = s.line(21, 41, 40, 41);
    const auto third = s.line(22, 42, 40, 42);
    s.stats.integrity_changes.push_back(joint(upper, lower, 20, 40));
    s.stats.integrity_changes.push_back(joint(lower, third, 21, 41));
    const auto suspects = s.suspects();
    SCI_CHECK(suspects.size() == std::size_t{1});
    SCI_CHECK(suspects[0].kind == SmoothingKind::Step);
    SCI_CHECK(suspects[0].count == 2);
    SCI_CHECK(suspects[0].lines.size() == std::size_t{3});
    SCI_CHECK(suspects[0].score == 240);

    Synthetic flush;
    const auto a = flush.line(20, 40, 40, 40);
    const auto b = flush.line(20, 41, 40, 41);
    flush.stats.integrity_changes.push_back(joint(a, b, 20, 40));
    SCI_CHECK(flush.suspects().empty());

    Synthetic same;
    const auto fill = same.line(18, 41, 22, 41);
    const auto top = same.line(20, 40, 40, 40);
    const auto under = same.line(21, 41, 40, 41);
    same.stats.integrity_changes.push_back(joint(top, under, 20, 40));
    SCI_CHECK(fill >= 0 && same.suspects().empty());
}

// Kind 2: a joint of the ends of an L that touch diagonally cuts the
// corner where the corner pixel shows another colour.
SCI_TEST(smoothing_corner_bevelled_by_a_joint) {
    Synthetic s;
    const auto down = s.line(50, 20, 50, 40);
    const auto right = s.line(51, 41, 70, 41);
    s.stats.integrity_changes.push_back(joint(down, right, 50, 40));
    s.stats.integrity_changes.push_back(joint(down, right, 50, 40));
    const auto suspects = s.suspects();
    SCI_CHECK(suspects.size() == std::size_t{1});
    SCI_CHECK(suspects[0].kind == SmoothingKind::Corner);
    SCI_CHECK(suspects[0].x == 50 && suspects[0].y == 40);
    SCI_CHECK(suspects[0].bend == 1000);
}

// Kind 2: an L of two long lines fitted by a spline that cuts the corner is
// a corner suspect; a stroke through the corner pixel is not.
SCI_TEST(smoothing_corner_rounded_by_a_fit) {
    for (const bool rounded : {true, false}) {
        Synthetic s;
        const auto down = s.line(30, 10, 30, 20);
        const auto right = s.line(30, 20, 40, 20);
        sci::vector::FittedChain chain;
        chain.kind = sci::vector::FittedKind::Spline;
        chain.lines = {down, right};
        s.stats.fitted_chains.push_back(chain);
        const Vec2 corner = rounded ? Vec2{.x = 31.5, .y = 19.5} : centre(30, 20);
        s.stats.fitted_strokes.push_back({.primitive = down, .points = {centre(30, 10), corner}});
        s.stats.fitted_strokes.push_back({.primitive = right, .points = {corner, centre(40, 20)}});
        const auto suspects = s.suspects();
        SCI_CHECK(count_kind(suspects, SmoothingKind::Corner) == (rounded ? std::size_t{1} : std::size_t{0}));
        if (rounded) {
            SCI_CHECK(suspects[0].x == 30);
            SCI_CHECK(suspects[0].y == 20);
            SCI_CHECK(suspects[0].bend == 1000);
            SCI_CHECK(suspects[0].deviation == 1414);
        }
    }
}

// Kind 2: a spline through many short lines turning evenly (raster
// staircasing of a curve) is not a corner, however it is fitted.
SCI_TEST(smoothing_corner_needs_a_concentrated_turn) {
    Synthetic s;
    const std::array<std::pair<int, int>, 6> points{{{10, 10}, {14, 10}, {17, 11}, {19, 13}, {20, 16}, {20, 20}}};
    sci::vector::FittedChain chain;
    chain.kind = sci::vector::FittedKind::Spline;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto l = s.line(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second);
        chain.lines.push_back(l);
        // A stroke well off every vertex.
        s.stats.fitted_strokes.push_back(
            {.primitive = l, .points = {{.x = 0, .y = 100}, {.x = 1, .y = 100}}});
    }
    s.stats.fitted_chains.push_back(chain);
    SCI_CHECK(count_kind(s.suspects(), SmoothingKind::Corner) == std::size_t{0});
}

// The vertices of a half circle of radius 13 about pixel (100, 80), exactly
// on it (Pythagorean triples).
constexpr std::array<std::pair<int, int>, 6> half_circle{
    {{87, 80}, {88, 75}, {95, 68}, {105, 68}, {112, 75}, {113, 80}}};

// Kind 3: a half circle drawn as five lines (vertices on the circle) is a
// curve suspect while the HD strokes are the lines (a polygon); when the
// strokes follow the circle, it is not.
SCI_TEST(smoothing_curve_drawn_as_polygon) {
    const auto& points = half_circle;
    for (const bool polygon : {true, false}) {
        Synthetic s;
        std::vector<std::int32_t> lines;
        for (std::size_t i = 1; i < points.size(); ++i) {
            lines.push_back(s.line(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second));
        }
        if (!polygon) {
            // The strokes on the circle about (100.5, 80.5), by the
            // rational parametrisation (no trigonometry): every line gets
            // the whole half circle.
            std::vector<Vec2> stroke;
            for (int k = 0; k <= 80; ++k) {
                const double t = k <= 40 ? k / 40.0 : 40.0 / (80 - k + 1e-9);
                const double d = 1 + (t * t);
                stroke.push_back({.x = 100.5 - (13 * (1 - (t * t)) / d), .y = 80.5 - (13 * 2 * t / d)});
            }
            for (const auto l : lines) {
                s.stats.fitted_strokes.push_back({.primitive = l, .points = stroke});
            }
            sci::vector::FittedChain chain;
            chain.kind = sci::vector::FittedKind::Arc;
            chain.lines = lines;
            s.stats.fitted_chains.push_back(chain);
        }
        const auto suspects = s.suspects();
        SCI_CHECK(count_kind(suspects, SmoothingKind::Curve) == (polygon ? std::size_t{1} : std::size_t{0}));
        if (polygon) {
            SCI_CHECK(suspects[0].count == 5);
            SCI_CHECK(suspects[0].bend > 120 && suspects[0].bend < 160);
            SCI_CHECK(suspects[0].deviation >= 300);
        }
    }
}

// Organic lines are left out: the same polygon in a scene-model tree, or
// in a chain hint labelled as a branch.
SCI_TEST(smoothing_leaves_out_organic_lines) {
    const auto& points = half_circle;
    Synthetic s;
    std::vector<sci::vector::LineRef> commands;
    for (std::size_t i = 1; i < points.size(); ++i) {
        commands.push_back(static_cast<sci::vector::LineRef>(
            s.line(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second)));
    }
    sci::vector::PictureHints hints;
    sci::vector::ChainHint chain;
    chain.id = "c";
    chain.lines = commands;
    chain.label = "Tree branch, left";
    hints.chains.push_back(chain);
    const std::array<const sci::vector::PictureHints*, 1> pointers{&hints};
    SCI_CHECK(s.suspects(pointers).empty());
    SCI_CHECK(sci::render::is_organic_label("oak foliage outline"));
    SCI_CHECK(sci::render::is_organic_label("Rock"));
    SCI_CHECK(!sci::render::is_organic_label("mansion window"));
    SCI_CHECK(!sci::render::is_organic_label("stone path border"));

    sci::vector::PictureHints model;
    sci::vector::ObjectHint object;
    object.id = "oak";
    object.main.id = "oak";
    object.main.type = sci::vector::ObjectType::Tree;
    sci::vector::PartHint part;
    part.id = "crown";
    sci::vector::PrimitiveHint primitive;
    primitive.id = "crown-outline";
    primitive.kind = sci::vector::PrimitiveKind::Spline;
    primitive.commands = commands;
    part.primitives.push_back(primitive);
    object.main.parts.push_back(part);
    model.objects.push_back(object);
    const std::array<const sci::vector::PictureHints*, 1> model_pointers{&model};
    SCI_CHECK(s.suspects(model_pointers).empty());
    SCI_CHECK(s.suspects().size() == std::size_t{1});
}
