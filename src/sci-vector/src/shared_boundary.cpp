// SPDX-License-Identifier: GPL-3.0-or-later

#include "shared_boundary.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace sci::vector::detail {

namespace {

// Positions closer than this (original pixels) are the same point.
constexpr double same_point = 1e-6;
// A vertex where one other straight line meets moves along that line at
// most this far (original pixels); otherwise to the nearest point.
constexpr double max_slide = 1.5;
// A point another line or shape fixed may lie this far off a piece
// (original pixels).
constexpr double max_off_piece = 0.25;
// Orientations this small count as collinear (rounding of moved points).
constexpr double collinear = 1e-9;

double cross(Vec2 a, Vec2 b, Vec2 c) {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

double distance(Vec2 a, Vec2 b) {
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)));
}

// A position on a polyline: chord index plus the fraction along it.
struct OnPiece {
    Vec2 point;
    double param = 0.0;
};

OnPiece nearest_on(const std::vector<Vec2>& piece, Vec2 p) {
    OnPiece best{.point = piece.front(), .param = 0.0};
    double best_d = distance(p, piece.front());
    for (std::size_t k = 0; k + 1 < piece.size(); ++k) {
        const Vec2 a = piece[k];
        const Vec2 b = piece[k + 1];
        const double len2 = ((b.x - a.x) * (b.x - a.x)) + ((b.y - a.y) * (b.y - a.y));
        double f = 0.0;
        if (len2 > 0.0) {
            f = std::clamp((((p.x - a.x) * (b.x - a.x)) + ((p.y - a.y) * (b.y - a.y))) / len2, 0.0, 1.0);
        }
        const Vec2 q{.x = a.x + (f * (b.x - a.x)), .y = a.y + (f * (b.y - a.y))};
        if (const double d = distance(p, q); d < best_d) {
            best = OnPiece{.point = q, .param = static_cast<double>(k) + f};
            best_d = d;
        }
    }
    return best;
}

// The point at fraction t (0 to 1) of the piece's length.
OnPiece at_fraction(const std::vector<Vec2>& piece, double t) {
    double total = 0.0;
    for (std::size_t k = 0; k + 1 < piece.size(); ++k) {
        total += distance(piece[k], piece[k + 1]);
    }
    double left = std::clamp(t, 0.0, 1.0) * total;
    for (std::size_t k = 0; k + 1 < piece.size(); ++k) {
        const double len = distance(piece[k], piece[k + 1]);
        if (left <= len && len > 0.0) {
            const double f = left / len;
            return OnPiece{
                .point =
                    {
                        .x = piece[k].x + (f * (piece[k + 1].x - piece[k].x)),
                        .y = piece[k].y + (f * (piece[k + 1].y - piece[k].y)),
                    },
                .param = static_cast<double>(k) + f,
            };
        }
        left -= len;
    }
    return OnPiece{.point = piece.back(), .param = static_cast<double>(piece.size() - 1)};
}

// Where the line through p in direction d meets the piece, nearest to p,
// within max_slide.
std::optional<OnPiece> slide_onto(const std::vector<Vec2>& piece, Vec2 p, Vec2 d) {
    const double len = std::sqrt((d.x * d.x) + (d.y * d.y));
    std::optional<OnPiece> best;
    double best_s = max_slide;
    for (std::size_t k = 0; k + 1 < piece.size(); ++k) {
        const Vec2 a = piece[k];
        const Vec2 e{.x = piece[k + 1].x - a.x, .y = piece[k + 1].y - a.y};
        const double den = (d.x * e.y) - (d.y * e.x);
        if (den == 0.0) {
            continue;
        }
        // p + s d = a + f e.
        const double s = (((a.x - p.x) * e.y) - ((a.y - p.y) * e.x)) / den;
        const double f = (((a.x - p.x) * d.y) - ((a.y - p.y) * d.x)) / den;
        if (f < 0.0 || f > 1.0) {
            continue;
        }
        if (const double slide = std::abs(s) * len; slide <= best_s) {
            best_s = slide;
            best = OnPiece{.point = {.x = p.x + (s * d.x), .y = p.y + (s * d.y)}, .param = static_cast<double>(k) + f};
        }
    }
    return best;
}

// A monotonic stand-in for the angle of a direction, in [0, 4).
double pseudo_angle(Vec2 d) {
    const double p = d.y / (std::abs(d.x) + std::abs(d.y));
    if (d.x < 0.0) {
        return 2.0 - p;
    }
    return d.y < 0.0 ? 4.0 + p : p;
}

using Bounds = CourseBox;

Bounds box_of(const std::vector<Vec2>& points) {
    Bounds b{.x0 = points.front().x, .y0 = points.front().y, .x1 = points.front().x, .y1 = points.front().y};
    for (const Vec2 p : points) {
        b.x0 = std::min(b.x0, p.x);
        b.y0 = std::min(b.y0, p.y);
        b.x1 = std::max(b.x1, p.x);
        b.y1 = std::max(b.y1, p.y);
    }
    return b;
}

bool chords_cross(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const double o1 = cross(a, b, c);
    const double o2 = cross(a, b, d);
    const double o3 = cross(c, d, a);
    const double o4 = cross(c, d, b);
    return ((o1 < -collinear && o2 > collinear) || (o1 > collinear && o2 < -collinear)) &&
           ((o3 < -collinear && o4 > collinear) || (o3 > collinear && o4 < -collinear));
}

bool courses_cross(const std::vector<Vec2>& s, const std::vector<Vec2>& t) {
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
        for (std::size_t j = 0; j + 1 < t.size(); ++j) {
            if (chords_cross(s[i], s[i + 1], t[j], t[j + 1])) {
                return true;
            }
        }
    }
    return false;
}

// Whether p lies strictly inside the closed polygon (crossing number;
// points on its boundary may count either way).
bool inside(const std::vector<Vec2>& polygon, Vec2 p) {
    bool in = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x = a.x + ((p.y - a.y) * (b.x - a.x) / (b.y - a.y));
            if (p.x < x) {
                in = !in;
            }
        }
    }
    return in;
}

// The direction a course leaves its first point in, if it has length.
std::optional<Vec2> leaving(const std::vector<Vec2>& course) {
    for (std::size_t k = 1; k < course.size(); ++k) {
        if (distance(course[k], course.front()) > same_point) {
            return Vec2{.x = course[k].x - course.front().x, .y = course[k].y - course.front().y};
        }
    }
    return std::nullopt;
}

// A course that leaves the end point of another course at almost the same
// direction (an arch springing from its jamb, two curves leaving one
// vertex) can cross it by a hair. Moves the points of `course` (oriented
// from the shared point) that lie on the wrong side of `other` (oriented
// from it too; `side` is the sign of the side `course` lay on before) just
// onto the right side, if none is farther than max_off_piece from it;
// nothing otherwise.
std::optional<std::vector<Vec2>> push_off(const std::vector<Vec2>& course, const std::vector<Vec2>& other,
                                          double side) {
    constexpr double step = 0.25;   // Original pixels between the points of a pushed course.
    constexpr double margin = 1e-6; // How far onto the right side.
    std::vector<Vec2> dense{course.front()};
    for (std::size_t k = 0; k + 1 < course.size(); ++k) {
        const double len = distance(course[k], course[k + 1]);
        const auto n = static_cast<std::size_t>(std::ceil(len / step));
        for (std::size_t j = 1; j <= n; ++j) {
            const double f = static_cast<double>(j) / static_cast<double>(n);
            dense.push_back(Vec2{
                .x = course[k].x + (f * (course[k + 1].x - course[k].x)),
                .y = course[k].y + (f * (course[k + 1].y - course[k].y)),
            });
        }
    }
    bool pushed = false;
    for (std::size_t i = 1; i < dense.size(); ++i) {
        const Vec2 p = dense[i];
        const OnPiece q = nearest_on(other, p);
        const auto k = std::min(static_cast<std::size_t>(q.param), other.size() - 2);
        const Vec2 d{.x = other[k + 1].x - other[k].x, .y = other[k + 1].y - other[k].y};
        const double len = std::sqrt((d.x * d.x) + (d.y * d.y));
        if (len == 0.0 || ((d.x * (p.y - other[k].y)) - (d.y * (p.x - other[k].x))) * side > collinear) {
            continue;
        }
        // The far end is a vertex: it stays.
        if (i + 1 == dense.size() || distance(p, q.point) > max_off_piece) {
            return std::nullopt;
        }
        dense[i] = Vec2{.x = q.point.x - (side * margin * d.y / len), .y = q.point.y + (side * margin * d.x / len)};
        pushed = true;
    }
    if (!pushed) {
        return std::nullopt;
    }
    return dense;
}

} // namespace

std::shared_ptr<const SharedBoundary::Index> SharedBoundary::make_index(const Arrangement& arrangement) {
    auto index = std::make_shared<Index>();
    index->points.reserve(arrangement.vertices.size());
    for (const core::RationalPoint& vertex : arrangement.vertices) {
        index->points.push_back(to_picture(vertex));
    }
    for (std::size_t v = 0; v < index->points.size(); ++v) {
        index->by_x.push_back(v);
    }
    std::ranges::stable_sort(index->by_x, {}, [&](std::size_t v) { return index->points[v].x; });
    index->leaving.resize(arrangement.vertices.size());
    for (std::size_t h = 0; h < arrangement.half_edges.size(); ++h) {
        index->leaving[static_cast<std::size_t>(arrangement.half_edges[h].origin)].push_back(h);
    }
    for (std::size_t e = 0; e < arrangement.sources.size(); ++e) {
        const std::int32_t source = arrangement.sources[e];
        if (source >= 0) {
            const auto s = static_cast<std::size_t>(source);
            if (index->of_source.size() <= s) {
                index->of_source.resize(s + 1);
            }
            index->of_source[s].push_back(e);
        } else if (source == shared_source) {
            index->shared.push_back(e);
        }
    }
    return index;
}

SharedBoundary::SharedBoundary(const Arrangement& arrangement, const std::vector<FittedChain>& fitted,
                               Analysis analysis)
    : arrangement_(&arrangement), analysis_(analysis), base_(arrangement.sources.size()),
      moved_(arrangement.vertices.size()), overrides_(arrangement.sources.size()) {
    for (const FittedChain& chain : fitted) {
        for (std::size_t i = 0; i < chain.edges.size(); ++i) {
            std::vector<Vec2> piece = chain.pieces[i];
            if (chain.half_edges[i] % 2 != 0) {
                std::ranges::reverse(piece);
            }
            base_[static_cast<std::size_t>(chain.edges[i])] = std::move(piece);
        }
    }
    for (std::size_t e = 0; e < base_.size(); ++e) {
        if (base_[e].empty()) {
            const auto vertex = [&](std::size_t h) {
                return to_picture(arrangement.vertices[static_cast<std::size_t>(arrangement.half_edges[h].origin)]);
            };
            base_[e] = {vertex(2 * e), vertex((2 * e) + 1)};
        }
    }
    course_ = base_;
    if (analysis_ == Analysis::Fast) {
        index_ = make_index(arrangement);
        boxes_.reserve(course_.size());
        for (const std::vector<Vec2>& course : course_) {
            boxes_.push_back(box_of(course));
        }
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): one shape's placement and its checks share its state.
std::string SharedBoundary::apply(const std::vector<BoundaryLine>& lines) {
    const Arrangement& arr = *arrangement_;
    const std::size_t edge_count = arr.sources.size();
    const auto origin = [&](std::size_t h) { return static_cast<std::size_t>(arr.half_edges[h].origin); };
    // The fast analysis's lookups (docs/hd-pipeline.md, "Room entry"); the
    // reference scans every vertex and half-edge. Both visit the same
    // vertices and half-edges in the same (ascending) order.
    const Index* index = index_.get();
    const auto point_of = [&](std::size_t v) {
        return index != nullptr ? index->points[v] : to_picture(arr.vertices[v]);
    };
    const auto position = [&](std::size_t v) { return moved_[v] ? *moved_[v] : point_of(v); };
    // The half-edges leaving a vertex.
    const auto leaving_from = [&](std::size_t v) {
        if (index != nullptr) {
            return index->leaving[v];
        }
        std::vector<std::size_t> found;
        for (std::size_t h = 0; h < arr.half_edges.size(); ++h) {
            if (origin(h) == v) {
                found.push_back(h);
            }
        }
        return found;
    };
    // The vertices whose points may lie within same_point of p: all for the
    // reference, those with x within same_point for the fast analysis.
    const auto vertices_near = [&](Vec2 p) {
        std::vector<std::size_t> found;
        if (index == nullptr) {
            for (std::size_t v = 0; v < arr.vertices.size(); ++v) {
                found.push_back(v);
            }
            return found;
        }
        const auto first = std::ranges::lower_bound(index->by_x, p.x - same_point, {},
                                                    [&](std::size_t v) { return index->points[v].x; });
        for (auto it = first; it != index->by_x.end() && index->points[*it].x <= p.x + same_point; ++it) {
            found.push_back(*it);
        }
        return found;
    };

    std::map<std::size_t, Vec2> placed;                  // Vertex -> its new point (this shape).
    std::map<std::size_t, std::vector<Vec2>> new_course; // Edge -> its new course (this shape).
    std::set<std::size_t> own_edges;                     // Edges of the shape's lines.

    // The line ends of the shape go to their pieces' ends, also where the
    // vertex lies on another of its lines (consecutive lines that overlap).
    std::map<std::size_t, Vec2> ends;
    for (const BoundaryLine& line : lines) {
        if (line.piece.size() < 2) {
            continue;
        }
        const bool reversed = distance(line.piece.back(), line.from) < distance(line.piece.front(), line.from);
        std::vector<std::size_t> near = vertices_near(line.from);
        std::ranges::move(vertices_near(line.to), std::back_inserter(near));
        std::ranges::stable_sort(near);
        const auto [last, end] = std::ranges::unique(near);
        near.erase(last, end);
        for (const std::size_t v : near) {
            const Vec2 p = point_of(v);
            if (distance(p, line.from) < same_point) {
                ends.try_emplace(v, reversed ? line.piece.back() : line.piece.front());
            } else if (distance(p, line.to) < same_point) {
                ends.try_emplace(v, reversed ? line.piece.front() : line.piece.back());
            }
        }
    }

    for (const BoundaryLine& line : lines) {
        std::vector<Vec2> piece = line.piece;
        if (piece.size() < 2) {
            return "a piece without length";
        }
        if (distance(piece.back(), line.from) < distance(piece.front(), line.from)) {
            std::ranges::reverse(piece);
        }
        const Vec2 a = line.from;
        const Vec2 b = line.to;
        const double len2 = ((b.x - a.x) * (b.x - a.x)) + ((b.y - a.y) * (b.y - a.y));
        const auto along = [&](Vec2 p) { return (((p.x - a.x) * (b.x - a.x)) + ((p.y - a.y) * (b.y - a.y))) / len2; };
        const auto on_line = [&](Vec2 p) { return std::abs(cross(a, b, p)) / std::sqrt(len2) < same_point; };
        // The line's edges: its own, and those it shares with lines over it.
        const auto is_line_edge = [&](std::size_t e) {
            const std::int32_t source = arr.sources[e];
            return source == line.source || (source == shared_source && on_line(point_of(origin(2 * e))) &&
                                             on_line(point_of(origin((2 * e) + 1))));
        };
        std::vector<std::size_t> edges;
        if (index != nullptr && line.source >= 0) {
            if (std::cmp_less(line.source, index->of_source.size())) {
                edges = index->of_source[static_cast<std::size_t>(line.source)];
            }
            std::ranges::copy_if(index->shared, std::back_inserter(edges), is_line_edge);
            std::ranges::stable_sort(edges);
        } else {
            for (std::size_t e = 0; e < edge_count; ++e) {
                if (is_line_edge(e)) {
                    edges.push_back(e);
                }
            }
        }
        if (edges.empty() || len2 == 0.0) {
            return "its line has no edges";
        }
        std::set<std::size_t> vertices;
        for (const std::size_t e : edges) {
            own_edges.insert(e);
            vertices.insert(origin(2 * e));
            vertices.insert(origin((2 * e) + 1));
        }
        const std::set<std::size_t> line_edges(edges.begin(), edges.end());
        // Each vertex's new point and its position along the piece. A
        // point already set (a line end of the shape, a crossing with an
        // earlier line, an earlier shape) is kept if it lies on the piece.
        const auto place = [&](std::size_t v, bool allow_slide) -> std::optional<OnPiece> {
            const Vec2 p = point_of(v);
            const double t = along(p);
            std::optional<Vec2> fixed = moved_[v];
            if (const auto found = placed.find(v); found != placed.end()) {
                fixed = found->second;
            } else if (const auto end = ends.find(v); end != ends.end()) {
                fixed = end->second;
            }
            if (distance(p, a) < same_point) {
                return OnPiece{.point = fixed.value_or(piece.front()), .param = 0.0};
            }
            if (distance(p, b) < same_point) {
                return OnPiece{.point = fixed.value_or(piece.back()), .param = static_cast<double>(piece.size() - 1)};
            }
            if (t < 0.0 || t > 1.0) {
                // On an extension that closes a gap: it stays.
                return OnPiece{
                    .point = fixed.value_or(p),
                    .param =
                        t < 0.0 ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity(),
                };
            }
            if (fixed) {
                const OnPiece q = nearest_on(piece, *fixed);
                if (distance(q.point, *fixed) > max_off_piece) {
                    return std::nullopt;
                }
                return OnPiece{.point = *fixed, .param = q.param};
            }
            if (allow_slide) {
                // The directions of the other edges here.
                std::vector<Vec2> others;
                for (const std::size_t h : leaving_from(v)) {
                    if (!line_edges.contains(h / 2)) {
                        const core::IntPoint d = arr.half_edges[h].direction;
                        others.push_back(Vec2{.x = static_cast<double>(d.x), .y = static_cast<double>(d.y)});
                    }
                }
                const bool one_line = !others.empty() && std::ranges::all_of(others, [&](Vec2 d) {
                    return (d.x * others.front().y) - (d.y * others.front().x) == 0.0;
                });
                if (one_line) {
                    if (const auto slid = slide_onto(piece, p, others.front())) {
                        return slid;
                    }
                }
            }
            return at_fraction(piece, t);
        };
        std::vector<std::pair<double, std::size_t>> by_t;
        by_t.reserve(vertices.size());
        for (const std::size_t v : vertices) {
            by_t.emplace_back(along(point_of(v)), v);
        }
        std::ranges::stable_sort(by_t);
        std::map<std::size_t, OnPiece> found_at;
        bool ordered = false;
        for (const bool allow_slide : {true, false}) {
            found_at.clear();
            for (const auto& [t, v] : by_t) {
                const auto q = place(v, allow_slide);
                if (!q) {
                    return "it meets another steered shape or line at a vertex off its curve";
                }
                found_at.emplace(v, *q);
            }
            ordered = true;
            for (std::size_t k = 1; k < by_t.size(); ++k) {
                ordered = ordered && found_at.at(by_t[k].second).param >= found_at.at(by_t[k - 1].second).param;
            }
            if (ordered) {
                break;
            }
        }
        if (!ordered) {
            return "the vertices along a line change their order";
        }
        std::map<std::size_t, double> param;
        for (const auto& [v, q] : found_at) {
            placed.try_emplace(v, q.point);
            param[v] = q.param;
        }
        for (const std::size_t e : edges) {
            std::size_t lo = origin(2 * e);
            std::size_t hi = origin((2 * e) + 1);
            const bool flipped = along(point_of(lo)) > along(point_of(hi));
            if (flipped) {
                std::swap(lo, hi);
            }
            std::vector<Vec2> course{placed.at(lo)};
            for (std::size_t j = 0; j < piece.size(); ++j) {
                const auto pj = static_cast<double>(j);
                if (pj > param[lo] && pj < param[hi]) {
                    course.push_back(piece[j]);
                }
            }
            course.push_back(placed.at(hi));
            if (flipped) {
                std::ranges::reverse(course);
            }
            if (const auto found = new_course.find(e); found != new_course.end() && found->second != course) {
                return "two of its lines run one edge differently";
            }
            new_course[e] = std::move(course);
        }
    }

    // The other edges at moved vertices end at the new points.
    for (const auto& [v, q] : placed) {
        for (const std::size_t h : leaving_from(v)) {
            if (own_edges.contains(h / 2)) {
                continue;
            }
            const std::size_t e = h / 2;
            auto [it, fresh] = new_course.try_emplace(e, course_[e]);
            std::vector<Vec2>& course = it->second;
            if (h % 2 != 0) {
                std::ranges::reverse(course);
            }
            // A fitted course drops its points near the old vertex, so that
            // it runs straight to the new one.
            const Vec2 old = course.front();
            const double reach = distance(old, q) + max_off_piece;
            std::size_t keep = 1;
            while (keep + 1 < course.size() && distance(course[keep], old) <= reach) {
                ++keep;
            }
            course.erase(course.begin() + 1, course.begin() + static_cast<std::ptrdiff_t>(keep));
            course.front() = q;
            if (h % 2 != 0) {
                std::ranges::reverse(course);
            }
        }
    }
    std::erase_if(new_course, [&](const auto& entry) { return entry.second == course_[entry.first]; });
    if (new_course.empty()) {
        return {};
    }

    const auto now = [&](std::size_t e) -> const std::vector<Vec2>& {
        const auto found = new_course.find(e);
        return found != new_course.end() ? found->second : course_[e];
    };
    // The box of an edge's course now: the fast analysis keeps the boxes of
    // the edges this shape does not move.
    std::vector<bool> is_new(edge_count, false);
    for (const auto& entry : new_course) {
        is_new[entry.first] = true;
    }
    const auto box_now = [&](std::size_t e) { return index != nullptr && !is_new[e] ? boxes_[e] : box_of(now(e)); };
    std::vector<bool> is_placed(arr.vertices.size(), false);
    for (const auto& entry : placed) {
        is_placed[entry.first] = true;
    }

    // A moved edge that crosses an edge leaving the same vertex at almost
    // the same direction is pushed just off it (at most max_off_piece).
    for (auto& [e, course] : new_course) {
        const Bounds box = box_of(course);
        for (std::size_t f = 0; f < edge_count; ++f) {
            if (f == e || !box.meets(box_now(f)) || !courses_cross(course, now(f))) {
                continue;
            }
            // The shared end, now and before, and the side e lay on.
            for (const bool e_front : {true, false}) {
                for (const bool f_front : {true, false}) {
                    std::vector<Vec2> mine = course;
                    std::vector<Vec2> theirs = now(f);
                    std::vector<Vec2> mine_old = course_[e];
                    std::vector<Vec2> theirs_old = course_[f];
                    if (!e_front) {
                        std::ranges::reverse(mine);
                        std::ranges::reverse(mine_old);
                    }
                    if (!f_front) {
                        std::ranges::reverse(theirs);
                        std::ranges::reverse(theirs_old);
                    }
                    if (distance(mine.front(), theirs.front()) > same_point ||
                        distance(mine_old.front(), theirs_old.front()) > same_point) {
                        continue;
                    }
                    const auto de = leaving(mine_old);
                    const auto df = leaving(theirs_old);
                    if (!de || !df) {
                        continue;
                    }
                    const double side = (df->x * de->y) - (df->y * de->x);
                    if (std::abs(side) <= collinear) {
                        continue;
                    }
                    if (auto pushed = push_off(mine, theirs, side > 0.0 ? 1.0 : -1.0)) {
                        if (!e_front) {
                            std::ranges::reverse(*pushed);
                        }
                        course = std::move(*pushed);
                    }
                }
            }
        }
    }
    // No moved edge crosses another edge.
    std::vector<Bounds> boxes(edge_count);
    for (std::size_t e = 0; e < edge_count; ++e) {
        boxes[e] = box_now(e);
    }
    for (const auto& [e, course] : new_course) {
        for (std::size_t f = 0; f < edge_count; ++f) {
            if (f != e && boxes[e].meets(boxes[f]) && courses_cross(course, now(f))) {
                return "its boundary would cross another edge";
            }
        }
    }
    // No vertex lies between a moved edge's old and new course.
    for (const auto& [e, course] : new_course) {
        std::vector<Vec2> swept = course_[e];
        swept.insert(swept.end(), course.rbegin(), course.rend());
        const Bounds area = box_of(swept);
        for (std::size_t v = 0; v < arr.vertices.size(); ++v) {
            if (v == origin(2 * e) || v == origin((2 * e) + 1) || is_placed[v]) {
                continue;
            }
            const Vec2 p = position(v);
            if (p.x >= area.x0 && p.x <= area.x1 && p.y >= area.y0 && p.y <= area.y1 && inside(swept, p)) {
                return "its boundary would pass over another vertex";
            }
        }
    }
    // The edges around each moved vertex keep their order.
    for (const auto& [v, q] : placed) {
        std::vector<std::pair<double, std::size_t>> before;
        std::vector<std::pair<double, std::size_t>> after;
        for (const std::size_t h : leaving_from(v)) {
            std::vector<Vec2> old_course = course_[h / 2];
            std::vector<Vec2> course = now(h / 2);
            if (h % 2 != 0) {
                std::ranges::reverse(old_course);
                std::ranges::reverse(course);
            }
            const auto d_old = leaving(old_course);
            const auto d_new = leaving(course);
            if (d_old && d_new) {
                before.emplace_back(pseudo_angle(*d_old), h);
                after.emplace_back(pseudo_angle(*d_new), h);
            }
        }
        if (before.size() < 3) {
            continue;
        }
        std::ranges::stable_sort(before);
        std::ranges::stable_sort(after);
        const auto start = std::ranges::find(after, before.front().second, &std::pair<double, std::size_t>::second);
        std::ranges::rotate(after, start);
        if (!std::ranges::equal(before, after, {}, &std::pair<double, std::size_t>::second,
                                &std::pair<double, std::size_t>::second)) {
            return "the edges at a vertex would change their order";
        }
    }

    for (auto& [e, course] : new_course) {
        if (index != nullptr) {
            boxes_[e] = box_of(course);
        }
        course_[e] = course;
        overrides_[e] = std::move(course);
    }
    for (const auto& [v, q] : placed) {
        moved_[v] = q;
    }
    changed_ = true;
    return {};
}

} // namespace sci::vector::detail
