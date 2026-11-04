// SPDX-License-Identifier: GPL-3.0-or-later

#include "scene_model.hpp"

#include "sci/pic/picture_maps.hpp"
#include "sci/vector/raster_1x.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <span>
#include <utility>

namespace sci::vector::detail {

namespace {

// Curves are flattened into chords this long at most (in original pixels),
// as in fitting.cpp.
constexpr double max_chord = 0.25;

Vec2 operator+(Vec2 a, Vec2 b) {
    return {.x = a.x + b.x, .y = a.y + b.y};
}

Vec2 operator-(Vec2 a, Vec2 b) {
    return {.x = a.x - b.x, .y = a.y - b.y};
}

Vec2 operator*(Vec2 a, double s) {
    return {.x = a.x * s, .y = a.y * s};
}

double dot(Vec2 a, Vec2 b) {
    return (a.x * b.x) + (a.y * b.y);
}

double length(Vec2 a) {
    return std::sqrt(dot(a, a));
}

Vec2 centre(pic::ScreenPoint p) {
    return {.x = p.x + 0.5, .y = (p.y - pic::picture_top) + 0.5};
}

bool touches(pic::ScreenPoint a, pic::ScreenPoint b) {
    return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;
}

// Whether more than two pixels of b lie next to pixels of a: the lines run
// side by side (a stroke drawn two lines wide, or stacked treads), they do
// not continue each other.
bool alongside(const SteerLine& a, const SteerLine& b) {
    const std::vector<std::uint32_t> pa = line_pixels_1x(a.from, a.to);
    const std::vector<std::uint32_t> pb = line_pixels_1x(b.from, b.to);
    int near = 0;
    for (const std::uint32_t q : pb) {
        const auto qx = static_cast<int>(q % picture_width);
        const auto qy = static_cast<int>(q / picture_width);
        near += std::ranges::any_of(pa, [&](std::uint32_t p) {
                    return std::abs(static_cast<int>(p % picture_width) - qx) <= 1 &&
                           std::abs(static_cast<int>(p / picture_width) - qy) <= 1;
                })
                    ? 1
                    : 0;
    }
    return near > 2;
}

// The lines oriented along one path, or nothing if they do not form one.
std::optional<std::vector<SteerLine>> as_path(std::span<const SteerLine> lines) {
    if (lines.empty()) {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (alongside(lines[i - 1], lines[i]) || alongside(lines[i], lines[i - 1])) {
            return std::nullopt;
        }
    }
    std::vector<SteerLine> path(lines.begin(), lines.end());
    if (path.size() >= 2) {
        const SteerLine& next = path[1];
        const auto meets_next = [&](pic::ScreenPoint p) { return touches(p, next.from) || touches(p, next.to); };
        if (!meets_next(path[0].to)) {
            if (!meets_next(path[0].from)) {
                return std::nullopt;
            }
            std::swap(path[0].from, path[0].to);
        }
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (!touches(path[i - 1].to, path[i].from)) {
            if (!touches(path[i - 1].to, path[i].to)) {
                return std::nullopt;
            }
            std::swap(path[i].from, path[i].to);
        }
    }
    return path;
}

// The vertices of a polyline: its ends and its corners, each corner at the
// nearest line end within one pixel (Chebyshev), else at its point.
std::optional<std::vector<Vec2>> polyline_shape(const SteerInput& input, std::span<const SteerLine> path) {
    std::vector<Vec2> shape{centre(path.front().from)};
    for (std::size_t k = 1; k + 1 < input.points.size(); ++k) {
        if (input.corner[k] == CornerKind::Rounded) {
            return std::nullopt;
        }
        const Vec2 point = input.points[k];
        Vec2 vertex = point;
        double best = 1.0;
        for (const SteerLine& line : path) {
            for (const pic::ScreenPoint end : {line.from, line.to}) {
                const Vec2 c = centre(end);
                const double d = std::max(std::abs(c.x - point.x), std::abs(c.y - point.y));
                if (d < best || (d == best && vertex == point)) {
                    best = d;
                    vertex = c;
                }
            }
        }
        if (shape.back() != vertex) {
            shape.push_back(vertex);
        }
    }
    const Vec2 end = centre(path.back().to);
    if (shape.back() != end) {
        shape.push_back(end);
    }
    return shape;
}

// Half of an ellipse from the path's start to its end, which are the ends
// of a diameter: the conic A x^2 + B xy + C y^2 = 1 about their midpoint,
// fitted by weighted least squares to the end (strongly), the anchors and
// the lines' pixel centres, then drawn through its conjugate diameters
// (only +, -, *, / and sqrt).
std::optional<std::vector<Vec2>> ellipse_shape(const SteerInput& input, std::span<const SteerLine> path) {
    constexpr double end_weight = 1e4;
    constexpr double anchor_weight = 1e2;
    const Vec2 start = centre(path.front().from);
    const Vec2 end = centre(path.back().to);
    const Vec2 middle = (start + end) * 0.5;
    const Vec2 u = end - middle;
    if (length(u) == 0) {
        return std::nullopt;
    }
    std::array<std::array<double, 4>, 3> m{};
    Vec2 side;
    const auto add = [&](Vec2 p, double weight) {
        const Vec2 q = p - middle;
        const std::array<double, 3> row{q.x * q.x, q.x * q.y, q.y * q.y};
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                m[r][c] += weight * row[r] * row[c];
            }
            m[r][3] += weight * row[r];
        }
    };
    add(end, end_weight);
    for (std::size_t k = 1; k + 1 < input.points.size(); ++k) {
        add(input.points[k], anchor_weight);
        side = side + (input.points[k] - middle);
    }
    for (const SteerLine& line : path) {
        pic::for_each_line_pixel(line.from, line.to, [&](int x, int y) {
            add(centre({.x = x, .y = y}), 1);
        });
    }
    // Cramer's rule.
    const auto det3 = [](const std::array<std::array<double, 4>, 3>& a, std::size_t replace) {
        const auto at = [&](std::size_t r, std::size_t c) { return a[r][c == replace ? 3 : c]; };
        return (at(0, 0) * ((at(1, 1) * at(2, 2)) - (at(1, 2) * at(2, 1)))) -
               (at(0, 1) * ((at(1, 0) * at(2, 2)) - (at(1, 2) * at(2, 0)))) +
               (at(0, 2) * ((at(1, 0) * at(2, 1)) - (at(1, 1) * at(2, 0))));
    };
    const double det = det3(m, 3);
    if (std::abs(det) < 1e-300) {
        return std::nullopt;
    }
    double a = det3(m, 0) / det;
    double b = det3(m, 1) / det;
    double c = det3(m, 2) / det;
    if (!(a > 0) || !(((4 * a * c) - (b * b)) > 0)) {
        return std::nullopt;
    }
    const auto form = [&](Vec2 p, Vec2 q) { return (a * p.x * q.x) + (b * 0.5 * ((p.x * q.y) + (p.y * q.x))) + (c * p.y * q.y); };
    const double scale = form(u, u);
    if (!(scale > 0)) {
        return std::nullopt;
    }
    a /= scale;
    b /= scale;
    c /= scale;
    // The conjugate semi-diameter: on the ellipse, along the tangent at u's end.
    const Vec2 qu{.x = (a * u.x) + (b * 0.5 * u.y), .y = (b * 0.5 * u.x) + (c * u.y)};
    Vec2 w{.x = -qu.y, .y = qu.x};
    w = w * (1 / std::sqrt(form(w, w)));
    if (dot(w, side) < 0) {
        w = w * -1;
    }
    // From (-1, 0) through (0, 1) to (1, 0) on the unit circle, mapped.
    const auto map = [&](Vec2 p) { return middle + (u * p.x) + (w * p.y); };
    std::vector<Vec2> shape{start};
    const std::array<Vec2, 3> quarters{Vec2{.x = -1, .y = 0}, Vec2{.x = 0, .y = 1}, Vec2{.x = 1, .y = 0}};
    for (std::size_t i = 0; i + 1 < quarters.size(); ++i) {
        std::vector<std::pair<Vec2, Vec2>> pending{{quarters[i], quarters[i + 1]}}; // Last first.
        while (!pending.empty()) {
            const auto [p, q] = pending.back();
            pending.pop_back();
            if (length(map(q) - map(p)) <= max_chord) {
                shape.push_back(map(q));
                continue;
            }
            const Vec2 half = (p + q) * 0.5;
            const Vec2 on_circle = half * (1 / length(half));
            pending.emplace_back(on_circle, q);
            pending.emplace_back(p, on_circle);
        }
    }
    shape.back() = end;
    return shape;
}

// Cuts the shape at the points nearest to the cut points, in order along
// it, into cuts.size() + 1 pieces.
std::vector<std::vector<Vec2>> split(const std::vector<Vec2>& shape, std::span<const Vec2> cuts) {
    std::vector<std::vector<Vec2>> pieces;
    std::vector<Vec2> current{shape.front()};
    std::size_t segment = 0;
    double cut = 0;
    for (const Vec2 target : cuts) {
        std::size_t best_segment = segment;
        double best_t = cut;
        double best = 1e300;
        for (std::size_t j = segment; j + 1 < shape.size(); ++j) {
            const Vec2 d = shape[j + 1] - shape[j];
            const double length_squared = dot(d, d);
            double t = length_squared == 0 ? 0 : dot(target - shape[j], d) / length_squared;
            t = std::clamp(t, j == segment ? cut : 0.0, 1.0);
            const double distance = dot(shape[j] + (d * t) - target, shape[j] + (d * t) - target);
            if (distance < best) {
                best = distance;
                best_segment = j;
                best_t = t;
            }
        }
        for (std::size_t j = segment + 1; j <= best_segment; ++j) {
            if (current.back() != shape[j]) {
                current.push_back(shape[j]);
            }
        }
        const Vec2 point = shape[best_segment] + ((shape[best_segment + 1] - shape[best_segment]) * best_t);
        if (current.back() != point) {
            current.push_back(point);
        }
        pieces.push_back(std::move(current));
        current = {point};
        segment = best_segment;
        cut = best_t;
    }
    for (std::size_t j = segment + 1; j < shape.size(); ++j) {
        if (current.back() != shape[j]) {
            current.push_back(shape[j]);
        }
    }
    pieces.push_back(std::move(current));
    return pieces;
}

} // namespace

Steered steer(const SteerInput& input) {
    Steered result;
    if (input.kind != PrimitiveKind::Line && input.kind != PrimitiveKind::Polyline &&
        input.kind != PrimitiveKind::Ellipse) {
        return result;
    }
    const std::optional<std::vector<SteerLine>> path = as_path(input.lines);
    if (!path) {
        return result;
    }
    std::optional<std::vector<Vec2>> shape;
    switch (input.kind) {
    case PrimitiveKind::Line:
        shape = std::vector<Vec2>{centre(path->front().from), centre(path->back().to)};
        break;
    case PrimitiveKind::Polyline:
        shape = polyline_shape(input, *path);
        break;
    case PrimitiveKind::Ellipse:
        if (input.points.size() < 3 || input.points.front() == input.points.back()) {
            return result; // A closed ellipse is fitted as a chain (section 6).
        }
        shape = ellipse_shape(input, *path);
        if (!shape) {
            result.applicable = true;
            result.reason = "no ellipse fits the lines";
            return result;
        }
        break;
    default:
        break;
    }
    if (!shape) {
        return result;
    }
    result.applicable = true;
    std::vector<Vec2> joints;
    for (std::size_t i = 0; i + 1 < path->size(); ++i) {
        joints.push_back((centre((*path)[i].to) + centre((*path)[i + 1].from)) * 0.5);
    }
    std::vector<std::vector<Vec2>> pieces = split(*shape, joints);
    // The round trip (A4 A.6) over the whole primitive.
    std::vector<std::uint32_t> original;
    std::vector<std::uint32_t> reconstructed;
    for (std::size_t i = 0; i < path->size(); ++i) {
        const std::vector<std::uint32_t> drawn = line_pixels_1x((*path)[i].from, (*path)[i].to);
        original.insert(original.end(), drawn.begin(), drawn.end());
        const std::vector<std::uint32_t> back = rasterize_1x(pieces[i]);
        reconstructed.insert(reconstructed.end(), back.begin(), back.end());
    }
    for (auto* pixels : {&original, &reconstructed}) {
        std::ranges::stable_sort(*pixels);
        const auto [first, last] = std::ranges::unique(*pixels);
        pixels->erase(first, last);
    }
    result.deviation = raster_deviation(original, reconstructed);
    if (result.deviation > round_trip_limit) {
        result.reason = std::format("round trip deviation {}", result.deviation);
        return result;
    }
    // The pieces follow the input's lines, not the path's orientation.
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if ((*path)[i].from != input.lines[i].from || (*path)[i].to != input.lines[i].to) {
            std::ranges::reverse(pieces[i]);
        }
    }
    result.pieces = std::move(pieces);
    return result;
}

std::vector<StepCut> step_cuts(const SteerInput& input) {
    std::vector<StepCut> cuts;
    if (input.kind != PrimitiveKind::Polyline) {
        return cuts;
    }
    for (std::size_t i = 0; i < input.lines.size(); ++i) {
        const SteerLine& line = input.lines[i];
        const bool horizontal = line.from.y == line.to.y && line.from.x != line.to.x;
        const bool vertical = line.from.x == line.to.x && line.from.y != line.to.y;
        if (!horizontal && !vertical) {
            continue;
        }
        for (const auto& [end, other] : {std::pair{line.from, line.to}, std::pair{line.to, line.from}}) {
            const Vec2 c = centre(end);
            bool step = false;
            for (std::size_t k = 0; k < input.points.size(); ++k) {
                step = step || (input.corner[k] == CornerKind::Step && input.points[k] == c);
            }
            if (!step) {
                continue;
            }
            const double y = end.y - pic::picture_top;
            Cut cut;
            if (horizontal) {
                cut = end.x < other.x ? Cut{.point = {.x = static_cast<double>(end.x), .y = y}, .normal = {.x = -1, .y = 0}}
                                      : Cut{.point = {.x = end.x + 1.0, .y = y}, .normal = {.x = 1, .y = 0}};
            } else {
                cut = end.y < other.y ? Cut{.point = {.x = static_cast<double>(end.x), .y = y}, .normal = {.x = 0, .y = -1}}
                                      : Cut{.point = {.x = static_cast<double>(end.x), .y = y + 1}, .normal = {.x = 0, .y = 1}};
            }
            const Vec2 edge = horizontal ? Vec2{.x = cut.point.x, .y = c.y} : Vec2{.x = c.x, .y = cut.point.y};
            cuts.push_back(StepCut{.line = i, .end = c, .edge = edge, .cut = cut});
        }
    }
    return cuts;
}

} // namespace sci::vector::detail
