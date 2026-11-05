// SPDX-License-Identifier: GPL-3.0-or-later
//
// The stroke integrity pass of the vector path (spec section 6.10,
// addendum A1) and the checks that measure it (sci/render/integrity.hpp).

#include "pic_builder.hpp"
#include "sci/core/executor.hpp"
#include "sci/pic/draw_pic.hpp"
#include "sci/pic/parse_pic.hpp"
#include "sci/render/integrity.hpp"
#include "sci/vector/build_scene.hpp"
#include "sci_test.hpp"
#include "stroke_integrity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

using sci::test::PicBuilder;
using sci::vector::IntegrityChangeKind;
using sci::vector::Plane;

namespace {

constexpr std::uint8_t black = 0;
constexpr std::uint8_t blue = 1;
constexpr std::uint8_t red = 4;

struct Drawn {
    sci::pic::PictureMaps maps;
    sci::pic::DrawTrace trace;
};

Drawn draw(const std::vector<std::uint8_t>& data) {
    Drawn drawn;
    const auto commands = sci::pic::parse_pic(data);
    SCI_CHECK(commands.has_value());
    if (commands) {
        SCI_CHECK(sci::pic::draw_picture(*commands, drawn.maps, {}, &drawn.trace).has_value());
    }
    return drawn;
}

sci::render::Integrity check(const Drawn& drawn, const sci::vector::Scene& scene) {
    sci::core::SerialExecutor executor;
    return sci::render::check_integrity(scene, drawn.maps, drawn.trace, executor);
}

int changes_of(const sci::vector::SceneStats& stats, IntegrityChangeKind kind) {
    return static_cast<int>(
        std::ranges::count_if(stats.integrity_changes, [&](const auto& change) { return change.kind == kind; }));
}

// The strokes of a primitive (trace index), connectors included.
std::vector<sci::vector::Stroke*> strokes_of(sci::vector::Scene& scene, std::int32_t primitive) {
    std::vector<sci::vector::Stroke*> strokes;
    for (auto& layer : scene.layers) {
        if (auto* stroke = std::get_if<sci::vector::Stroke>(&layer); stroke != nullptr && stroke->source == primitive) {
            strokes.push_back(stroke);
        }
    }
    return strokes;
}

} // namespace

SCI_TEST(a_line_ending_on_a_contour_is_clipped_at_its_near_edge) {
    // A frame line, then a line that ends on the pixel next to it.
    const Drawn drawn = draw(PicBuilder().color(black).line(100, 40, 100, 120).color(red).line(40, 80, 99, 80).end());
    auto built = sci::vector::build_scene(drawn.maps, drawn.trace, Plane::Visual);
    SCI_CHECK(changes_of(built.stats, IntegrityChangeKind::ClippedEnd) == 1);
    const auto strokes = strokes_of(built.scene, 1);
    SCI_CHECK(!strokes.empty());
    if (strokes.empty()) {
        return;
    }
    // The end lies on the frame's centreline, cut along its left edge.
    const auto& points = strokes.front()->points;
    const auto end = std::ranges::max(points, {}, &sci::vector::Vec2::x);
    SCI_CHECK(end.x == 100.5);
    SCI_CHECK(strokes.front()->cuts.size() == 1);
    SCI_CHECK(strokes.front()->cuts.front().point.x == 100.0);
    const auto integrity = check(drawn, built.scene);
    SCI_CHECK(integrity.contacts == 1 && integrity.overshoots == 0);
    SCI_CHECK(integrity.broken_joints == 0);

    // Without the cut, the end reaches into the frame: an overshoot.
    strokes.front()->cuts.clear();
    const auto uncut = check(drawn, built.scene);
    SCI_CHECK(uncut.overshoots == 1);
    SCI_CHECK(std::ranges::any_of(uncut.cases, [](const auto& c) {
        return c.kind == sci::render::IntegrityKind::Overshoot && c.primitive == 1 && c.other == 0 && c.x == 99;
    }));
}

SCI_TEST(a_corner_between_diagonal_line_ends_is_closed) {
    // Two lines whose ends are on diagonally touching pixels.
    const Drawn drawn = draw(PicBuilder().color(red).line(40, 50, 80, 50).line(81, 51, 81, 100).end());
    const auto built = sci::vector::build_scene(drawn.maps, drawn.trace, Plane::Visual);
    SCI_CHECK(changes_of(built.stats, IntegrityChangeKind::ClosedJoint) >= 1);
    const auto integrity = check(drawn, built.scene);
    SCI_CHECK(integrity.joints >= 2);
    SCI_CHECK(integrity.broken_joints == 0 && integrity.fragments == 0);
}

SCI_TEST(a_boundary_drawn_twice_shows_once_in_the_colour_seen) {
    // Two outlines along the same pixels for half their length: the later
    // one is what the player saw there, so the earlier stroke is removed
    // along it.
    const Drawn drawn = draw(PicBuilder().color(red).line(40, 60, 120, 60).color(blue).line(80, 60, 160, 60).end());
    const auto built = sci::vector::build_scene(drawn.maps, drawn.trace, Plane::Visual);
    SCI_CHECK(changes_of(built.stats, IntegrityChangeKind::MergedRun) == 1);
    const auto integrity = check(drawn, built.scene);
    SCI_CHECK(integrity.shared_pairs == 1 && integrity.doubled == 0);
    SCI_CHECK(integrity.passes());
}

SCI_TEST(the_checks_find_a_fragment_and_a_split_line) {
    const Drawn drawn = draw(PicBuilder().color(red).line(40, 60, 120, 60).end());
    auto built = sci::vector::build_scene(drawn.maps, drawn.trace, Plane::Visual);
    SCI_CHECK(check(drawn, built.scene).passes());
    auto strokes = strokes_of(built.scene, 0);
    SCI_CHECK(strokes.size() == 1);
    if (strokes.size() != 1) {
        return;
    }
    // A gap in the middle splits the line's pixels in two pieces; a piece
    // away from them is a fragment.
    strokes.front()->points = {{.x = 40.5, .y = 60.5}, {.x = 70.5, .y = 60.5}};
    const auto piece = [](double from, double to, double y) {
        return sci::vector::Stroke{
            .source = 0,
            .points = {{.x = from, .y = y}, {.x = to, .y = y}},
            .value = red,
            .fitted = false,
            .cuts = {},
        };
    };
    built.scene.layers.emplace_back(piece(90.5, 120.5, 60.5));
    built.scene.layers.emplace_back(piece(90.5, 92.5, 30.5));
    const auto integrity = check(drawn, built.scene);
    SCI_CHECK(integrity.split_groups == 1 && integrity.hidden_groups == 0);
    SCI_CHECK(integrity.fragments == 1);
    SCI_CHECK(!integrity.passes());
}

namespace {

// Two lines of a steered shape meeting at an inner joint at (20, 20), the
// shape bulging left there, and a later line ending on the pixel beside
// the joint, under the shape's stroke. `decided`: the customer decided it.
struct OnTop {
    std::vector<sci::vector::Layer> layers;
    sci::vector::SceneStats stats;
};

OnTop end_under_a_shape(bool decided) {
    const auto index = [](int x, int y) {
        return (static_cast<std::size_t>(y) * sci::vector::picture_width) + static_cast<std::size_t>(x);
    };
    const auto stroke = [](std::int32_t source, sci::vector::Vec2 a, sci::vector::Vec2 b) {
        return sci::vector::Stroke{.source = source, .points = {a, b}, .value = red, .fitted = true, .cuts = {}};
    };
    std::vector<sci::vector::detail::StrokeLine> lines(3);
    std::vector<std::int32_t> owners(sci::vector::picture_pixels, -1);
    for (int y = 10; y <= 19; ++y) {
        lines[0].path.push_back(index(20, y));
        owners[index(20, y)] = 0;
    }
    for (int y = 20; y <= 30; ++y) {
        lines[1].path.push_back(index(20, y));
        owners[index(20, y)] = 1;
    }
    for (int x = 10; x <= 19; ++x) {
        lines[2].path.push_back(index(x, 20));
        owners[index(x, 20)] = 2;
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        lines[i].primitive = static_cast<std::int32_t>(i);
    }
    lines[0].fixed = {index(20, 19)};
    lines[0].inner = lines[0].fixed;
    lines[1].fixed = {index(20, 20)};
    lines[1].inner = lines[1].fixed;
    lines[0].decided = decided;
    lines[1].decided = decided;
    OnTop result;
    result.layers = {stroke(0, {.x = 20.5, .y = 10.5}, {.x = 19.8, .y = 20}),
                     stroke(1, {.x = 19.8, .y = 20}, {.x = 20.5, .y = 30.5}),
                     stroke(2, {.x = 10.5, .y = 20.5}, {.x = 19.5, .y = 20.5})};
    sci::vector::detail::apply_stroke_integrity(result.layers, lines, owners, result.stats);
    return result;
}

} // namespace

SCI_TEST(a_decided_shape_stays_on_top_of_a_line_ending_under_it) {
    // Decided: the ending line is cut at the shape's near edge, so its
    // round end no longer reaches onto the shape's stroke.
    const OnTop decided = end_under_a_shape(true);
    SCI_CHECK(changes_of(decided.stats, IntegrityChangeKind::ClippedEnd) == 1);
    bool cut = false;
    for (const auto& layer : decided.layers) {
        const auto* stroke = std::get_if<sci::vector::Stroke>(&layer);
        if (stroke != nullptr && stroke->source == 2 && !stroke->cuts.empty()) {
            cut = !stroke->kept({.x = 19.5, .y = 20.5}) && stroke->kept({.x = 19.2, .y = 20.5}) &&
                  stroke->kept({.x = 12, .y = 20.5});
        }
    }
    SCI_CHECK(cut);
    // Not decided: the joint is a line end there, and nothing changes.
    const OnTop open = end_under_a_shape(false);
    SCI_CHECK(changes_of(open.stats, IntegrityChangeKind::ClippedEnd) == 0);
}
