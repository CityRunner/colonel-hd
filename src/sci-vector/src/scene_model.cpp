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
        near += std::ranges::any_of(pa,
                                    [&](std::uint32_t p) {
                                        return std::abs(static_cast<int>(p % picture_width) - qx) <= 1 &&
                                               std::abs(static_cast<int>(p / picture_width) - qy) <= 1;
                                    })
                    ? 1
                    : 0;
    }
    return near > 2;
}

// Whether two end pixels meet: they touch, or, with `reach` 2, they are
// at most two pixels apart (Chebyshev) or joined by a run of the bridge
// pixels (dots of the primitive), each at most two pixels from the next.
bool within(pic::ScreenPoint a, pic::ScreenPoint b, int reach, std::span<const pic::ScreenPoint> bridges) {
    const auto near = [&](pic::ScreenPoint p, pic::ScreenPoint q) {
        return std::abs(p.x - q.x) <= reach && std::abs(p.y - q.y) <= reach;
    };
    if (near(a, b)) {
        return true;
    }
    if (reach < 2) {
        return false;
    }
    std::vector<bool> used(bridges.size(), false);
    std::vector<pic::ScreenPoint> pending{a};
    while (!pending.empty()) {
        const pic::ScreenPoint p = pending.back();
        pending.pop_back();
        if (p != a && near(p, b)) {
            return true;
        }
        for (std::size_t i = 0; i < bridges.size(); ++i) {
            if (!used[i] && near(p, bridges[i])) {
                used[i] = true;
                pending.push_back(bridges[i]);
            }
        }
    }
    return false;
}

// The lines oriented along one path, or nothing if they do not form one.
// With `reach` 2, consecutive lines may leave a gap of one pixel between
// their ends (a pixel another line drew over), or a gap the primitive's
// dots bridge.
std::optional<std::vector<SteerLine>> as_path(std::span<const SteerLine> lines, int reach = 1,
                                              std::span<const pic::ScreenPoint> bridges = {}) {
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
        const auto meets_next = [&](pic::ScreenPoint p) {
            return within(p, next.from, reach, bridges) || within(p, next.to, reach, bridges);
        };
        if (!meets_next(path[0].to)) {
            if (!meets_next(path[0].from)) {
                return std::nullopt;
            }
            std::swap(path[0].from, path[0].to);
        }
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (!within(path[i - 1].to, path[i].from, reach, bridges)) {
            if (!within(path[i - 1].to, path[i].to, reach, bridges)) {
                return std::nullopt;
            }
            std::swap(path[i].from, path[i].to);
        }
    }
    return path;
}

// Normal equations of a linear least-squares problem in three unknowns,
// the right-hand side in column 3.
using Normal3 = std::array<std::array<double, 4>, 3>;

// Cramer's rule: the determinant of m with column `replace` taken from the
// right-hand side; replace = 3 gives the system's own determinant.
double det3(const Normal3& a, std::size_t replace) {
    const auto at = [&](std::size_t r, std::size_t c) { return a[r][c == replace ? 3 : c]; };
    return (at(0, 0) * ((at(1, 1) * at(2, 2)) - (at(1, 2) * at(2, 1)))) -
           (at(0, 1) * ((at(1, 0) * at(2, 2)) - (at(1, 2) * at(2, 0)))) +
           (at(0, 2) * ((at(1, 0) * at(2, 1)) - (at(1, 1) * at(2, 0))));
}

// Adds a weighted row (row . unknowns = rhs) to the normal equations.
void add_row(Normal3& m, const std::array<double, 3>& row, double rhs, double weight) {
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            m[r][c] += weight * row[r] * row[c];
        }
        m[r][3] += weight * row[r] * rhs;
    }
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
// With `minimax_rounds` (a primitive with a wider tolerance), the pixels'
// weights are then adjusted by Lawson's rule (each multiplied by its
// residual) so that the fit tends to the smallest largest deviation.
std::optional<std::vector<Vec2>> ellipse_shape(const SteerInput& input, std::span<const SteerLine> path,
                                               int minimax_rounds = 0) {
    constexpr double end_weight = 1e4;
    constexpr double anchor_weight = 1e2;
    const Vec2 start = centre(path.front().from);
    const Vec2 end = centre(path.back().to);
    const Vec2 middle = (start + end) * 0.5;
    const Vec2 u = end - middle;
    if (length(u) == 0) {
        return std::nullopt;
    }
    Vec2 side;
    for (std::size_t k = 1; k + 1 < input.points.size(); ++k) {
        side = side + (input.points[k] - middle);
    }
    std::vector<Vec2> pixels;
    for (const SteerLine& line : path) {
        pic::for_each_line_pixel(line.from, line.to, [&](int x, int y) { pixels.push_back(centre({.x = x, .y = y})); });
    }
    std::vector<double> weights(pixels.size(), 1.0);
    const auto row = [&](Vec2 p) {
        const Vec2 q = p - middle;
        return std::array<double, 3>{q.x * q.x, q.x * q.y, q.y * q.y};
    };
    std::array<double, 3> conic{};
    for (int round = 0; round <= minimax_rounds; ++round) {
        Normal3 m{};
        add_row(m, row(end), 1, end_weight);
        for (std::size_t k = 1; k + 1 < input.points.size(); ++k) {
            add_row(m, row(input.points[k]), 1, anchor_weight);
        }
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            add_row(m, row(pixels[i]), 1, weights[i]);
        }
        const double det = det3(m, 3);
        if (std::abs(det) < 1e-300) {
            return std::nullopt;
        }
        conic = {det3(m, 0) / det, det3(m, 1) / det, det3(m, 2) / det};
        double total = 0;
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const std::array<double, 3> r = row(pixels[i]);
            const double residual = std::abs((conic[0] * r[0]) + (conic[1] * r[1]) + (conic[2] * r[2]) - 1);
            weights[i] *= residual;
            total += weights[i];
        }
        if (!(total > 0)) {
            break;
        }
        for (double& w : weights) {
            w *= static_cast<double>(pixels.size()) / total;
        }
    }
    double a = conic[0];
    double b = conic[1];
    double c = conic[2];
    if (!(a > 0) || !(((4 * a * c) - (b * b)) > 0)) {
        return std::nullopt;
    }
    const auto form = [&](Vec2 p, Vec2 q) {
        return (a * p.x * q.x) + (b * 0.5 * ((p.x * q.y) + (p.y * q.x))) + (c * p.y * q.y);
    };
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

// Solves the n x n system m (right-hand side in column n) by Gaussian
// elimination with partial pivoting; nothing if it is singular.
template <std::size_t N>
std::optional<std::array<double, N>> solve(std::array<std::array<double, N + 1>, N> m) {
    for (std::size_t c = 0; c < N; ++c) {
        std::size_t pivot = c;
        for (std::size_t r = c + 1; r < N; ++r) {
            if (std::abs(m[r][c]) > std::abs(m[pivot][c])) {
                pivot = r;
            }
        }
        if (!(std::abs(m[pivot][c]) > 1e-300)) {
            return std::nullopt;
        }
        std::swap(m[c], m[pivot]);
        for (std::size_t r = c + 1; r < N; ++r) {
            const double f = m[r][c] / m[c][c];
            for (std::size_t k = c; k <= N; ++k) {
                m[r][k] -= f * m[c][k];
            }
        }
    }
    std::array<double, N> x{};
    for (std::size_t c = N; c-- > 0;) {
        double sum = m[c][N];
        for (std::size_t k = c + 1; k < N; ++k) {
            sum -= m[c][k] * x[k];
        }
        x[c] = sum / m[c][c];
    }
    return x;
}

// A whole ellipse: its centre and two conjugate semi-diameters.
struct EllipseFrame {
    Vec2 centre;
    Vec2 u;
    Vec2 w;
};

// The ellipse A x^2 + B xy + C y^2 + D x + E y = 1 about the centroid of
// the lines' pixel centres, fitted by weighted least squares to the anchor
// points and those pixel centres; nothing if the conic is no ellipse.
std::optional<EllipseFrame> fit_closed_ellipse(const SteerInput& input) {
    constexpr double anchor_weight = 1e2;
    std::vector<Vec2> pixels;
    for (const SteerLine& line : input.lines) {
        pic::for_each_line_pixel(line.from, line.to, [&](int x, int y) { pixels.push_back(centre({.x = x, .y = y})); });
    }
    if (pixels.size() < 5) {
        return std::nullopt;
    }
    Vec2 origin;
    for (const Vec2 p : pixels) {
        origin = origin + p;
    }
    origin = origin * (1 / static_cast<double>(pixels.size()));
    std::array<std::array<double, 6>, 5> m{};
    const auto add = [&](Vec2 p, double weight) {
        const Vec2 q = p - origin;
        const std::array<double, 5> row{q.x * q.x, q.x * q.y, q.y * q.y, q.x, q.y};
        for (std::size_t r = 0; r < 5; ++r) {
            for (std::size_t c = 0; c < 5; ++c) {
                m[r][c] += weight * row[r] * row[c];
            }
            m[r][5] += weight * row[r];
        }
    };
    for (std::size_t k = 0; k + 1 < input.points.size(); ++k) { // The last point repeats the first.
        add(input.points[k], anchor_weight);
    }
    for (const Vec2 p : pixels) {
        add(p, 1);
    }
    const auto conic = solve<5>(m);
    if (!conic) {
        return std::nullopt;
    }
    const auto [a, b, c, d, e] = *conic;
    const double det = (4 * a * c) - (b * b);
    if (!(a > 0) || !(det > 0)) {
        return std::nullopt;
    }
    // The centre, where the gradient vanishes, and the form about it.
    const Vec2 offset{.x = ((b * e) - (2 * c * d)) / det, .y = ((b * d) - (2 * a * e)) / det};
    const double level = 1 + (a * offset.x * offset.x) + (b * offset.x * offset.y) + (c * offset.y * offset.y);
    if (!(level > 0)) {
        return std::nullopt;
    }
    const auto form = [&](Vec2 p, Vec2 q) {
        return ((a * p.x * q.x) + (b * 0.5 * ((p.x * q.y) + (p.y * q.x))) + (c * p.y * q.y)) / level;
    };
    Vec2 u{.x = 1, .y = 0};
    u = u * (1 / std::sqrt(form(u, u)));
    // The conjugate semi-diameter: along the tangent at u's end.
    const Vec2 qu{.x = (a * u.x) + (b * 0.5 * u.y), .y = (b * 0.5 * u.x) + (c * u.y)};
    Vec2 w{.x = -qu.y, .y = qu.x};
    w = w * (1 / std::sqrt(form(w, w)));
    const EllipseFrame frame{.centre = origin + offset, .u = u, .w = w};
    if (!std::isfinite(frame.centre.x + frame.centre.y + u.x + u.y + w.x + w.y)) {
        return std::nullopt;
    }
    return frame;
}

// The point of the unit circle that p maps to in the ellipse's frame (p
// projected radially onto the ellipse); nothing at the centre.
std::optional<Vec2> on_unit_circle(const EllipseFrame& frame, Vec2 p) {
    const Vec2 q = p - frame.centre;
    const double det = (frame.u.x * frame.w.y) - (frame.u.y * frame.w.x);
    const Vec2 s{
        .x = ((q.x * frame.w.y) - (q.y * frame.w.x)) / det,
        .y = ((frame.u.x * q.y) - (frame.u.y * q.x)) / det,
    };
    const double r = length(s);
    if (!(r > 0) || !std::isfinite(r)) {
        return std::nullopt;
    }
    return s * (1 / r);
}

// A whole ellipse (first and last point equal): each line's piece is the
// part of one ellipse, fitted to all the lines, between the line's ends
// projected onto it, the shorter way round. Ends of two lines on touching
// pixels meet halfway, so the outline stays connected where it was drawn
// so; where the lines leave a gap (another object bounds the area there),
// the gap stays. The lines need not form one path.
std::optional<std::vector<std::vector<Vec2>>> closed_ellipse_pieces(const SteerInput& input) {
    const std::optional<EllipseFrame> frame = fit_closed_ellipse(input);
    if (!frame) {
        return std::nullopt;
    }
    const auto end_point = [&](std::size_t line, bool to) {
        const pic::ScreenPoint end = to ? input.lines[line].to : input.lines[line].from;
        for (std::size_t other = 0; other < input.lines.size(); ++other) {
            if (other == line) {
                continue;
            }
            for (const pic::ScreenPoint e : {input.lines[other].from, input.lines[other].to}) {
                if (touches(end, e)) {
                    return (centre(end) + centre(e)) * 0.5;
                }
            }
        }
        return centre(end);
    };
    const auto map = [&](Vec2 p) { return frame->centre + (frame->u * p.x) + (frame->w * p.y); };
    std::vector<std::vector<Vec2>> pieces;
    for (std::size_t i = 0; i < input.lines.size(); ++i) {
        const auto a = on_unit_circle(*frame, end_point(i, false));
        const auto b = on_unit_circle(*frame, end_point(i, true));
        if (!a || !b || dot(*a, *b) < 0) {
            return std::nullopt; // A line spans half the ellipse or more: not a piece of its outline.
        }
        std::vector<Vec2> piece{map(*a)};
        std::vector<std::pair<Vec2, Vec2>> pending{{*a, *b}}; // Last first.
        while (!pending.empty()) {
            const auto [p, q] = pending.back();
            pending.pop_back();
            const Vec2 half = (p + q) * 0.5;
            if (length(map(q) - map(p)) <= max_chord || length(half) == 0) {
                piece.push_back(map(q));
                continue;
            }
            const Vec2 mid = half * (1 / length(half));
            pending.emplace_back(mid, q);
            pending.emplace_back(p, mid);
        }
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

// u turned by a quarter turn, clockwise on screen (y down) for turn = 1.
Vec2 quarter_turn(Vec2 u, double turn) {
    return Vec2{.x = -u.y * turn, .y = u.x * turn};
}

double cross(Vec2 a, Vec2 b) {
    return (a.x * b.y) - (a.y * b.x);
}

// A circular arc from the path's start to its end: the circle
// x^2 + y^2 + D x + E y + F = 0 about their midpoint, fitted by weighted
// least squares to the ends (strongly), the anchors and the lines' pixel
// centres, and followed from the start the way the first line leaves it,
// to the end. It may turn by more than half a circle (a hook). Flattened
// by bisecting quarter turns (only +, -, *, / and sqrt).
std::optional<std::vector<Vec2>> arc_shape(const SteerInput& input, std::span<const SteerLine> path) {
    constexpr double end_weight = 1e4;
    constexpr double anchor_weight = 1e2;
    const Vec2 start = centre(path.front().from);
    const Vec2 end = centre(path.back().to);
    const Vec2 middle = (start + end) * 0.5;
    Normal3 m{};
    const auto add = [&](Vec2 p, double weight) {
        const Vec2 q = p - middle;
        add_row(m, {q.x, q.y, 1}, -dot(q, q), weight);
    };
    add(start, end_weight);
    add(end, end_weight);
    for (std::size_t k = 1; k + 1 < input.points.size(); ++k) {
        add(input.points[k], anchor_weight);
    }
    for (const SteerLine& line : path) {
        pic::for_each_line_pixel(line.from, line.to, [&](int x, int y) { add(centre({.x = x, .y = y}), 1); });
    }
    const double det = det3(m, 3);
    if (std::abs(det) < 1e-300) {
        return std::nullopt;
    }
    const Vec2 offset{.x = -det3(m, 0) / det * 0.5, .y = -det3(m, 1) / det * 0.5};
    const double radius_squared = dot(offset, offset) - (det3(m, 2) / det);
    if (!(radius_squared > 0)) {
        return std::nullopt;
    }
    const double radius = std::sqrt(radius_squared);
    const Vec2 o = middle + offset;
    const Vec2 a = (start - o) * (1 / length(start - o));
    const Vec2 b = (end - o) * (1 / length(end - o));
    if (!std::isfinite(a.x + a.y + b.x + b.y)) {
        return std::nullopt;
    }
    const double turn = dot(quarter_turn(a, 1), centre(path.front().to) - start) >= 0 ? 1 : -1;
    // Quarter turns from a until b lies within a quarter turn ahead.
    std::vector<Vec2> keys{a};
    while ((cross(keys.back(), b) * turn <= 0 || dot(keys.back(), b) < 0) && keys.size() < 5) {
        keys.push_back(quarter_turn(keys.back(), turn));
    }
    keys.push_back(b);
    const auto map = [&](Vec2 u) { return o + (u * radius); };
    std::vector<Vec2> shape{start};
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        std::vector<std::pair<Vec2, Vec2>> pending{{keys[i], keys[i + 1]}}; // Last first.
        while (!pending.empty()) {
            const auto [p, q] = pending.back();
            pending.pop_back();
            const Vec2 half = (p + q) * 0.5;
            if (length(map(q) - map(p)) <= max_chord || length(half) == 0) {
                shape.push_back(map(q));
                continue;
            }
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

// The shape a primitive asks for, along the path; nothing if the kind is
// not steered here, or (with a reason) if no shape of that kind fits.
std::optional<std::vector<Vec2>> shape_of(const SteerInput& input, std::span<const SteerLine> path,
                                          const std::vector<Vec2>* solved, std::string& reason,
                                          int minimax_rounds = 0) {
    if (solved != nullptr && (input.kind == PrimitiveKind::Line || input.kind == PrimitiveKind::Polyline)) {
        return *solved;
    }
    switch (input.kind) {
    case PrimitiveKind::Line:
        return std::vector<Vec2>{centre(path.front().from), centre(path.back().to)};
    case PrimitiveKind::Polyline:
        return polyline_shape(input, path);
    case PrimitiveKind::Ellipse: {
        if (input.points.size() < 3 || input.points.front() == input.points.back()) {
            return std::nullopt; // A whole ellipse is steered by closed_ellipse_pieces.
        }
        auto shape = ellipse_shape(input, path, minimax_rounds);
        if (!shape) {
            reason = "no ellipse fits the lines";
        }
        return shape;
    }
    case PrimitiveKind::Arc: {
        auto shape = arc_shape(input, path);
        if (!shape) {
            reason = "no circle fits the lines";
        }
        return shape;
    }
    default:
        return std::nullopt;
    }
}

// Where consecutive lines of the path meet: halfway between their end
// pixels, or at a polyline's corner within one pixel of that.
std::vector<Vec2> joints_of(const SteerInput& input, std::span<const SteerLine> path, const std::vector<Vec2>& shape,
                            bool solved) {
    std::vector<Vec2> joints;
    // A solved polyline with a vertex between every two lines meets there.
    if (solved && input.kind == PrimitiveKind::Polyline && shape.size() == path.size() + 1) {
        joints.assign(shape.begin() + 1, shape.end() - 1);
        return joints;
    }
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        Vec2 joint = (centre(path[i].to) + centre(path[i + 1].from)) * 0.5;
        if (input.kind == PrimitiveKind::Polyline) {
            const auto corner = std::ranges::find_if(shape.begin() + 1, shape.end() - 1, [&](Vec2 c) {
                return std::abs(c.x - joint.x) <= 1 && std::abs(c.y - joint.y) <= 1;
            });
            if (corner != shape.end() - 1) {
                joint = *corner;
            }
        }
        joints.push_back(joint);
    }
    return joints;
}

// The shape cut into one piece per line of the path. Where two lines leave
// a gap between their ends, the shape is cut at both ends (each projected
// onto it) and the part between stays open, as a whole ellipse leaves it.
std::vector<std::vector<Vec2>> pieces_of(const SteerInput& input, std::span<const SteerLine> path,
                                         const std::vector<Vec2>& shape, bool solved) {
    const std::vector<Vec2> joints = joints_of(input, path, shape, solved);
    std::vector<Vec2> cuts;
    std::vector<bool> gap_after; // Per cut: whether the part after it lies in a gap.
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        if (touches(path[i].to, path[i + 1].from)) {
            cuts.push_back(joints[i]);
            gap_after.push_back(false);
        } else {
            cuts.push_back(centre(path[i].to));
            cuts.push_back(centre(path[i + 1].from));
            gap_after.push_back(true);
            gap_after.push_back(false);
        }
    }
    std::vector<std::vector<Vec2>> split_pieces = split(shape, cuts);
    std::vector<std::vector<Vec2>> pieces;
    for (std::size_t k = 0; k < split_pieces.size(); ++k) {
        // Piece k follows cut k - 1; it is a gap if that cut opened one.
        if (k > 0 && gap_after[k - 1]) {
            continue;
        }
        pieces.push_back(std::move(split_pieces[k]));
    }
    return pieces;
}

// The round trip (A4 A.6) of the pieces against the path's lines.
int round_trip_of(std::span<const SteerLine> path, const std::vector<std::vector<Vec2>>& pieces) {
    std::vector<std::uint32_t> original;
    std::vector<std::uint32_t> reconstructed;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const std::vector<std::uint32_t> drawn = line_pixels_1x(path[i].from, path[i].to);
        original.insert(original.end(), drawn.begin(), drawn.end());
        const std::vector<std::uint32_t> back = rasterize_1x(pieces[i]);
        reconstructed.insert(reconstructed.end(), back.begin(), back.end());
    }
    for (auto* pixels : {&original, &reconstructed}) {
        std::ranges::stable_sort(*pixels);
        const auto [first, last] = std::ranges::unique(*pixels);
        pixels->erase(first, last);
    }
    return raster_deviation(original, reconstructed);
}

// The Chebyshev distance (the measure of A4 A.6) from p to segment a-b:
// the largest coordinate difference, minimised along the segment (a convex
// function of the position, so a ternary search finds it).
double chebyshev_to_segment(Vec2 p, Vec2 a, Vec2 b) {
    const auto at = [&](double t) {
        const Vec2 q = a + ((b - a) * t) - p;
        return std::max(std::abs(q.x), std::abs(q.y));
    };
    double low = 0;
    double high = 1;
    for (int i = 0; i < 60; ++i) {
        const double m1 = low + ((high - low) / 3);
        const double m2 = high - ((high - low) / 3);
        if (at(m1) <= at(m2)) {
            high = m2;
        } else {
            low = m1;
        }
    }
    return std::min({at(0), at(1), at((low + high) * 0.5)});
}

// The Chebyshev distance from p to the nearest of the polylines.
double distance_to_any(Vec2 p, const std::vector<std::vector<Vec2>>& polylines) {
    double best = 1e300;
    for (const std::vector<Vec2>& line : polylines) {
        for (std::size_t k = 0; k < line.size(); ++k) {
            best = std::min(best, chebyshev_to_segment(p, line[k], k + 1 < line.size() ? line[k + 1] : line[k]));
        }
    }
    return best;
}

// The largest Chebyshev distance (as A4 A.6 measures), both ways, between
// the lines' pixel centres (each line as the polyline through them) and the
// shape (all its pieces), in thousandths of a pixel, rounded up.
int continuous_deviation(std::span<const SteerLine> path, const std::vector<std::vector<Vec2>>& pieces) {
    std::vector<std::vector<Vec2>> drawn;
    for (const SteerLine& line : path) {
        std::vector<Vec2> centres;
        pic::for_each_line_pixel(line.from, line.to,
                                 [&](int x, int y) { centres.push_back(centre({.x = x, .y = y})); });
        drawn.push_back(std::move(centres));
    }
    double worst = 0;
    const std::vector<std::vector<Vec2>>& lines = drawn;
    for (const auto& [from, to] : {std::pair{&lines, &pieces}, std::pair{&pieces, &lines}}) {
        for (const std::vector<Vec2>& line : *from) {
            for (const Vec2 p : line) {
                worst = std::max(worst, distance_to_any(p, *to));
            }
        }
    }
    return static_cast<int>(std::ceil(worst * 1000));
}

// The round trip (A4 A.6): within round_trip_limit, or, for a primitive
// with a wider tolerance the customer approved, within that tolerance
// measured both on the 1x raster (rounded up to whole pixels) and between
// the pieces and the lines' pixel centres. Sets the deviation and, on
// failure, the reason.
bool passes_round_trip(const SteerInput& input, std::span<const SteerLine> path,
                       const std::vector<std::vector<Vec2>>& pieces, Steered& result) {
    result.deviation = round_trip_of(path, pieces);
    if (result.deviation <= round_trip_limit) {
        return true;
    }
    if (input.tolerance > tolerance_unit && result.deviation <= raster_reach(input.tolerance)) {
        const int continuous = continuous_deviation(path, pieces);
        if (continuous <= input.tolerance) {
            result.widened = true;
            return true;
        }
        result.reason = std::format("round trip deviation {} ({}.{:03} px, beyond the approved {}.{:03})",
                                    result.deviation, continuous / tolerance_unit, continuous % tolerance_unit,
                                    input.tolerance / tolerance_unit, input.tolerance % tolerance_unit);
        return false;
    }
    result.reason = std::format("round trip deviation {}", result.deviation);
    return false;
}

bool is_step_corner(const SteerInput& input, Vec2 point) {
    for (std::size_t k = 0; k < input.points.size(); ++k) {
        if (input.corner[k] == CornerKind::Step && input.points[k] == point) {
            return true;
        }
    }
    return false;
}

// The square end at `end` of a horizontal or vertical line whose other end
// is `other`.
StepCut square_end(std::size_t line, pic::ScreenPoint end, pic::ScreenPoint other, bool horizontal) {
    const Vec2 c = centre(end);
    const auto x = static_cast<double>(end.x);
    const auto y = static_cast<double>(end.y - pic::picture_top);
    Cut cut;
    if (horizontal) {
        cut = end.x < other.x ? Cut{.point = {.x = x, .y = y}, .normal = {.x = -1, .y = 0}}
                              : Cut{.point = {.x = x + 1, .y = y}, .normal = {.x = 1, .y = 0}};
    } else {
        cut = end.y < other.y ? Cut{.point = {.x = x, .y = y}, .normal = {.x = 0, .y = -1}}
                              : Cut{.point = {.x = x, .y = y + 1}, .normal = {.x = 0, .y = 1}};
    }
    const Vec2 edge = horizontal ? Vec2{.x = cut.point.x, .y = c.y} : Vec2{.x = c.x, .y = cut.point.y};
    return StepCut{.line = line, .end = c, .edge = edge, .cut = cut};
}

// An open ellipse or an arc may be steered over lines that leave gaps of
// one pixel between them, as a whole ellipse may (a pixel of the outline
// that another line drew over).
bool spans_gaps(const SteerInput& input) {
    return input.kind == PrimitiveKind::Arc || (input.kind == PrimitiveKind::Ellipse && input.points.size() >= 3 &&
                                                input.points.front() != input.points.back());
}

bool is_closed_ellipse(const SteerInput& input) {
    return input.kind == PrimitiveKind::Ellipse && input.points.size() >= 4 &&
           input.points.front() == input.points.back();
}

} // namespace

std::optional<std::vector<Vec2>> anchors(const SteerInput& input) {
    if (input.kind != PrimitiveKind::Line && input.kind != PrimitiveKind::Polyline) {
        return std::nullopt;
    }
    const std::optional<std::vector<SteerLine>> path = as_path(input.lines);
    if (!path) {
        return std::nullopt;
    }
    if (input.kind == PrimitiveKind::Line) {
        return std::vector<Vec2>{centre(path->front().from), centre(path->back().to)};
    }
    return polyline_shape(input, *path);
}

Steered steer(const SteerInput& input, const std::vector<Vec2>* solved) {
    Steered result;
    if (is_closed_ellipse(input)) {
        if (input.lines.empty()) {
            return result;
        }
        result.applicable = true;
        std::optional<std::vector<std::vector<Vec2>>> pieces = closed_ellipse_pieces(input);
        if (!pieces) {
            result.reason = "no ellipse fits the lines";
            return result;
        }
        if (!passes_round_trip(input, input.lines, *pieces, result)) {
            return result;
        }
        result.pieces = std::move(*pieces);
        return result;
    }
    std::optional<std::vector<SteerLine>> path = as_path(input.lines);
    if (!path && spans_gaps(input)) {
        path = as_path(input.lines, 2, input.dots);
    }
    if (!path) {
        return result;
    }
    const std::optional<std::vector<Vec2>> shape = shape_of(input, *path, solved, result.reason);
    if (!shape) {
        result.applicable = !result.reason.empty();
        return result;
    }
    result.applicable = true;
    std::vector<std::vector<Vec2>> pieces = pieces_of(input, *path, *shape, solved != nullptr);
    if (!passes_round_trip(input, *path, pieces, result)) {
        // A half ellipse with a wider tolerance: the fit nearest to minimax
        // may still lie within it.
        if (input.kind != PrimitiveKind::Ellipse || input.tolerance <= tolerance_unit) {
            return result;
        }
        constexpr int minimax_rounds = 40;
        const std::optional<std::vector<Vec2>> minimax = shape_of(input, *path, solved, result.reason, minimax_rounds);
        if (!minimax) {
            return result;
        }
        pieces = pieces_of(input, *path, *minimax, solved != nullptr);
        if (!passes_round_trip(input, *path, pieces, result)) {
            return result;
        }
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
        if (is_step_corner(input, centre(line.from))) {
            cuts.push_back(square_end(i, line.from, line.to, horizontal));
        }
        if (is_step_corner(input, centre(line.to))) {
            cuts.push_back(square_end(i, line.to, line.from, horizontal));
        }
    }
    return cuts;
}

} // namespace sci::vector::detail
