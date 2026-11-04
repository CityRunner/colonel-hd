// SPDX-License-Identifier: GPL-3.0-or-later

#include "relations.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <numeric>
#include <optional>
#include <utility>

namespace sci::vector::detail {

namespace {

// Gauss-Newton steps at most; linear relations need one.
constexpr int max_steps = 50;
// A constraint holds when its value (dimensionless or in pixels) is below.
constexpr double holds = 1e-9;
// A row is independent of the rows kept before it when this much of its
// length is left after removing their directions.
constexpr double independent = 1e-9;

// sum coeff * x[var] = 0.
struct Linear {
    std::vector<std::pair<std::size_t, double>> terms;
};

// The directions of two lines (from vertex a0 to a1 and b0 to b1) are
// parallel (cross product 0) or perpendicular (dot product 0), divided by
// their first lengths so that the value is dimensionless.
struct Bilinear {
    std::size_t a0 = 0;
    std::size_t a1 = 0;
    std::size_t b0 = 0;
    std::size_t b1 = 0;
    bool perpendicular = false;
    double scale = 1;
};

struct Constraints {
    std::vector<Linear> linear;
    std::vector<Bilinear> bilinear;
};

// The anchors as vertices 0..n-1; vertex v has x at 2v, y at 2v + 1.
struct Vertices {
    std::vector<std::size_t> offset; // Per primitive.
    std::size_t count = 0;

    explicit Vertices(const RelationProblem& problem) {
        for (const SolvePrimitive& primitive : problem.primitives) {
            offset.push_back(count);
            count += primitive.anchors.size();
        }
    }

    [[nodiscard]] std::size_t of(AnchorRef ref) const { return offset[ref.primitive] + ref.point; }
};

std::size_t find(std::vector<std::size_t>& parent, std::size_t v) {
    while (parent[v] != v) {
        parent[v] = parent[parent[v]];
        v = parent[v];
    }
    return v;
}

// The vertices of items, as groups of tied vertices (each by its
// representative, the smallest), in order of first appearance.
std::vector<std::size_t> groups_of(const RelationProblem& problem, const Vertices& vertices,
                                   const std::vector<std::size_t>& item_primitives, std::vector<std::size_t>& parent) {
    std::vector<std::size_t> groups;
    for (const std::size_t p : item_primitives) {
        for (std::size_t k = 0; k < problem.primitives[p].anchors.size(); ++k) {
            const std::size_t g = find(parent, vertices.of({.primitive = p, .point = k}));
            if (std::ranges::find(groups, g) == groups.end()) {
                groups.push_back(g);
            }
        }
    }
    return groups;
}

std::vector<std::size_t> flatten(const std::vector<std::vector<std::size_t>>& items) {
    std::vector<std::size_t> all;
    for (const auto& item : items) {
        all.insert(all.end(), item.begin(), item.end());
    }
    return all;
}

// sum over the item's vertices of x (or y) / count, as terms.
void add_centre(Linear& row, const RelationProblem& problem, const Vertices& vertices,
                const std::vector<std::size_t>& item, std::size_t coordinate, double sign) {
    std::size_t count = 0;
    for (const std::size_t p : item) {
        count += problem.primitives[p].anchors.size();
    }
    for (const std::size_t p : item) {
        for (std::size_t k = 0; k < problem.primitives[p].anchors.size(); ++k) {
            row.terms.emplace_back((2 * vertices.of({.primitive = p, .point = k})) + coordinate,
                                   sign / static_cast<double>(count));
        }
    }
}

// Mirror pairs about the axis (a vertical axis mirrors x): each group is
// paired with the group nearest to its mirror image about the groups' mean,
// and the pairing must be mutual; a group paired with itself lies on the
// axis. The constraints: paired groups at the same height (vertical axis),
// and every pair's midpoint (or lone group) on one line across the axis.
std::optional<std::string> add_symmetry(const RelationProblem& problem, const Vertices& vertices,
                                        const SolveRelation& relation, const std::vector<Vec2>& position,
                                        std::vector<std::size_t>& parent, Constraints& out) {
    const std::vector<std::size_t> groups = groups_of(problem, vertices, flatten(relation.items), parent);
    if (groups.size() < 2) {
        return "symmetry of fewer than two points";
    }
    const std::size_t across = relation.axis == Axis::Vertical ? 0 : 1; // The mirrored coordinate.
    const std::size_t along = 1 - across;
    const auto coordinate = [&](std::size_t g, std::size_t c) { return c == 0 ? position[g].x : position[g].y; };
    double axis = 0;
    for (const std::size_t g : groups) {
        axis += coordinate(g, across);
    }
    axis /= static_cast<double>(groups.size());
    std::vector<std::size_t> partner(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const double mirrored = (2 * axis) - coordinate(groups[i], across);
        double best = 1e300;
        for (std::size_t j = 0; j < groups.size(); ++j) {
            const double d_across = coordinate(groups[j], across) - mirrored;
            const double d_along = coordinate(groups[j], along) - coordinate(groups[i], along);
            const double d = (d_across * d_across) + (d_along * d_along);
            if (d < best) {
                best = d;
                partner[i] = j;
            }
        }
    }
    std::vector<Linear> middles; // x + x' (pairs) or 2x (on the axis), in the mirrored coordinate.
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const std::size_t j = partner[i];
        if (partner[j] != i) {
            return "symmetry: a point has no mirror partner";
        }
        if (j < i) {
            continue;
        }
        Linear middle;
        middle.terms.emplace_back((2 * groups[i]) + across, 1.0);
        middle.terms.emplace_back((2 * groups[j]) + across, 1.0);
        middles.push_back(std::move(middle));
        if (j != i) {
            out.linear.push_back(Linear{.terms = {{(2 * groups[i]) + along, 1.0}, {(2 * groups[j]) + along, -1.0}}});
        }
    }
    for (std::size_t k = 1; k < middles.size(); ++k) {
        Linear row = middles[k];
        for (const auto& [var, coeff] : middles.front().terms) {
            row.terms.emplace_back(var, -coeff);
        }
        out.linear.push_back(std::move(row));
    }
    return std::nullopt;
}

// The line items of a parallel or perpendicular relation: every primitive
// of every item must be a line.
std::optional<std::vector<std::size_t>> line_items(const RelationProblem& problem, const SolveRelation& relation) {
    std::vector<std::size_t> lines = flatten(relation.items);
    if (std::ranges::any_of(lines, [&](std::size_t p) { return problem.primitives[p].kind != PrimitiveKind::Line; })) {
        return std::nullopt;
    }
    return lines;
}

Bilinear bilinear(const RelationProblem& problem, const Vertices& vertices, std::size_t a, std::size_t b,
                  bool perpendicular) {
    const auto length = [&](std::size_t p) {
        const Vec2 from = problem.primitives[p].anchors.front();
        const Vec2 to = problem.primitives[p].anchors.back();
        return std::sqrt(((to.x - from.x) * (to.x - from.x)) + ((to.y - from.y) * (to.y - from.y)));
    };
    const std::size_t a_last = problem.primitives[a].anchors.size() - 1;
    const std::size_t b_last = problem.primitives[b].anchors.size() - 1;
    return Bilinear{
        .a0 = vertices.of({.primitive = a, .point = 0}),
        .a1 = vertices.of({.primitive = a, .point = a_last}),
        .b0 = vertices.of({.primitive = b, .point = 0}),
        .b1 = vertices.of({.primitive = b, .point = b_last}),
        .perpendicular = perpendicular,
        .scale = length(a) * length(b),
    };
}

// The constraints of one relation; a reason if it cannot be stated.
std::optional<std::string> add_relation(const RelationProblem& problem, const Vertices& vertices,
                                        const SolveRelation& relation, const std::vector<Vec2>& position,
                                        std::vector<std::size_t>& parent, Constraints& out) {
    switch (relation.kind) {
    case GeometricKind::Symmetry:
        return add_symmetry(problem, vertices, relation, position, parent, out);
    case GeometricKind::Parallel:
    case GeometricKind::Perpendicular: {
        const auto lines = line_items(problem, relation);
        const bool perpendicular = relation.kind == GeometricKind::Perpendicular;
        if (!lines || lines->size() < 2 || (perpendicular && lines->size() != 2)) {
            return std::format("{} needs line primitives", perpendicular ? "perpendicular" : "parallel");
        }
        for (std::size_t k = 1; k < lines->size(); ++k) {
            const Bilinear b = bilinear(problem, vertices, lines->front(), (*lines)[k], perpendicular);
            if (!(b.scale > 0)) {
                return "a line of no length";
            }
            out.bilinear.push_back(b);
        }
        return std::nullopt;
    }
    case GeometricKind::Aligned: {
        const std::size_t c = relation.axis == Axis::Vertical ? 0 : 1;
        for (std::size_t k = 1; k < relation.items.size(); ++k) {
            Linear row;
            add_centre(row, problem, vertices, relation.items[k], c, 1);
            add_centre(row, problem, vertices, relation.items.front(), c, -1);
            out.linear.push_back(std::move(row));
        }
        return std::nullopt;
    }
    case GeometricKind::EqualSpacing: {
        // Across the first line of the first item, as drawn.
        const auto first = std::ranges::find_if(
            relation.items.front(), [&](std::size_t p) { return problem.primitives[p].kind == PrimitiveKind::Line; });
        if (first == relation.items.front().end()) {
            return "equal spacing needs a line in its first item";
        }
        const Vec2 from = problem.primitives[*first].anchors.front();
        const Vec2 to = problem.primitives[*first].anchors.back();
        const double length = std::sqrt(((to.x - from.x) * (to.x - from.x)) + ((to.y - from.y) * (to.y - from.y)));
        if (!(length > 0)) {
            return "a line of no length";
        }
        const Vec2 normal{.x = -(to.y - from.y) / length, .y = (to.x - from.x) / length};
        // n . (c[k+1] - c[k]) = n . (c[k] - c[k-1]).
        for (std::size_t k = 1; k + 1 < relation.items.size(); ++k) {
            Linear row;
            for (std::size_t c = 0; c < 2; ++c) {
                const double n = c == 0 ? normal.x : normal.y;
                add_centre(row, problem, vertices, relation.items[k + 1], c, n);
                add_centre(row, problem, vertices, relation.items[k], c, -2 * n);
                add_centre(row, problem, vertices, relation.items[k - 1], c, n);
            }
            out.linear.push_back(std::move(row));
        }
        return std::nullopt;
    }
    case GeometricKind::Tangent:
        return "tangent is not solved yet";
    }
    return "unknown relation";
}

// The value and gradient (dense, over 2n unknowns) of each constraint at x.
void evaluate(const Constraints& constraints, const std::vector<double>& x, std::vector<double>& value,
              std::vector<std::vector<double>>& gradient) {
    const std::size_t n = x.size();
    value.clear();
    gradient.clear();
    for (const Linear& row : constraints.linear) {
        std::vector<double> g(n, 0.0);
        double v = 0;
        for (const auto& [var, coeff] : row.terms) {
            g[var] += coeff;
            v += coeff * x[var];
        }
        value.push_back(v);
        gradient.push_back(std::move(g));
    }
    for (const Bilinear& b : constraints.bilinear) {
        const double ax = x[2 * b.a1] - x[2 * b.a0];
        const double ay = x[(2 * b.a1) + 1] - x[(2 * b.a0) + 1];
        const double bx = x[2 * b.b1] - x[2 * b.b0];
        const double by = x[(2 * b.b1) + 1] - x[(2 * b.b0) + 1];
        std::vector<double> g(n, 0.0);
        // d/d(a) and d/d(b) of cross = ax by - ay bx, or dot = ax bx + ay by.
        const double dax = b.perpendicular ? bx : by;
        const double day = b.perpendicular ? by : -bx;
        const double dbx = b.perpendicular ? ax : -ay;
        const double dby = b.perpendicular ? ay : ax;
        g[2 * b.a1] += dax / b.scale;
        g[2 * b.a0] -= dax / b.scale;
        g[(2 * b.a1) + 1] += day / b.scale;
        g[(2 * b.a0) + 1] -= day / b.scale;
        g[2 * b.b1] += dbx / b.scale;
        g[2 * b.b0] -= dbx / b.scale;
        g[(2 * b.b1) + 1] += dby / b.scale;
        g[(2 * b.b0) + 1] -= dby / b.scale;
        value.push_back((b.perpendicular ? (ax * bx) + (ay * by) : (ax * by) - (ay * bx)) / b.scale);
        gradient.push_back(std::move(g));
    }
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// The rows independent of those before them (modified Gram-Schmidt, in
// order), by index.
std::vector<std::size_t> independent_rows(const std::vector<std::vector<double>>& rows) {
    std::vector<std::vector<double>> basis;
    std::vector<std::size_t> kept;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::vector<double> v = rows[i];
        const double norm = std::sqrt(dot(v, v));
        if (!(norm > 0)) {
            continue;
        }
        for (int pass = 0; pass < 2; ++pass) {
            for (const auto& q : basis) {
                const double c = dot(v, q);
                for (std::size_t k = 0; k < v.size(); ++k) {
                    v[k] -= c * q[k];
                }
            }
        }
        const double left = std::sqrt(dot(v, v));
        if (left > independent * norm) {
            for (double& e : v) {
                e /= left;
            }
            basis.push_back(std::move(v));
            kept.push_back(i);
        }
    }
    return kept;
}

// Solves m x = b by Gaussian elimination with partial pivoting.
std::optional<std::vector<double>> solve_dense(std::vector<std::vector<double>> m, std::vector<double> b) {
    const std::size_t n = b.size();
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t pivot = c;
        for (std::size_t r = c + 1; r < n; ++r) {
            if (std::abs(m[r][c]) > std::abs(m[pivot][c])) {
                pivot = r;
            }
        }
        if (!(std::abs(m[pivot][c]) > 1e-300)) {
            return std::nullopt;
        }
        std::swap(m[c], m[pivot]);
        std::swap(b[c], b[pivot]);
        for (std::size_t r = c + 1; r < n; ++r) {
            const double f = m[r][c] / m[c][c];
            for (std::size_t k = c; k < n; ++k) {
                m[r][k] -= f * m[c][k];
            }
            b[r] -= f * b[c];
        }
    }
    std::vector<double> x(n, 0.0);
    for (std::size_t c = n; c-- > 0;) {
        double sum = b[c];
        for (std::size_t k = c + 1; k < n; ++k) {
            sum -= m[c][k] * x[k];
        }
        x[c] = sum / m[c][c];
    }
    return x;
}

} // namespace

RelationSolution solve_relations(const RelationProblem& problem) {
    RelationSolution solution;
    const Vertices vertices(problem);
    std::vector<double> x0(2 * vertices.count);
    std::vector<Vec2> position(vertices.count);
    for (std::size_t p = 0; p < problem.primitives.size(); ++p) {
        for (std::size_t k = 0; k < problem.primitives[p].anchors.size(); ++k) {
            const std::size_t v = vertices.of({.primitive = p, .point = k});
            position[v] = problem.primitives[p].anchors[k];
            x0[2 * v] = position[v].x;
            x0[(2 * v) + 1] = position[v].y;
        }
    }
    // Tied vertices: one group each, at their mean, represented by the smallest.
    std::vector<std::size_t> parent(vertices.count);
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    Constraints constraints;
    for (const auto& [a, b] : problem.ties) {
        const std::size_t ra = find(parent, vertices.of(a));
        const std::size_t rb = find(parent, vertices.of(b));
        if (ra != rb) {
            parent[std::max(ra, rb)] = std::min(ra, rb);
        }
    }
    std::vector<Vec2> sum(vertices.count);
    std::vector<double> count(vertices.count, 0.0);
    for (std::size_t v = 0; v < vertices.count; ++v) {
        const std::size_t r = find(parent, v);
        sum[r].x += position[v].x;
        sum[r].y += position[v].y;
        count[r] += 1;
        if (r != v) {
            for (std::size_t c = 0; c < 2; ++c) {
                constraints.linear.push_back(Linear{.terms = {{(2 * v) + c, 1.0}, {(2 * r) + c, -1.0}}});
            }
        }
    }
    std::vector<Vec2> group_position = position;
    for (std::size_t v = 0; v < vertices.count; ++v) {
        if (count[v] > 0) {
            group_position[v] = {.x = sum[v].x / count[v], .y = sum[v].y / count[v]};
        }
    }
    for (const SolveRelation& relation : problem.relations) {
        if (auto reason = add_relation(problem, vertices, relation, group_position, parent, constraints)) {
            solution.reason = std::move(*reason);
            return solution;
        }
    }
    std::vector<double> x = x0;
    std::vector<double> value;
    std::vector<std::vector<double>> gradient;
    for (int step = 0; step < max_steps; ++step) {
        evaluate(constraints, x, value, gradient);
        const std::vector<std::size_t> rows = independent_rows(gradient);
        // (J J^T) lambda = -g(x) - J (x0 - x); x' = x0 + J^T lambda.
        std::vector<std::vector<double>> m(rows.size(), std::vector<double>(rows.size(), 0.0));
        std::vector<double> b(rows.size(), 0.0);
        std::vector<double> back(x.size());
        for (std::size_t k = 0; k < x.size(); ++k) {
            back[k] = x0[k] - x[k];
        }
        for (std::size_t i = 0; i < rows.size(); ++i) {
            for (std::size_t j = 0; j < rows.size(); ++j) {
                m[i][j] = dot(gradient[rows[i]], gradient[rows[j]]);
            }
            b[i] = -value[rows[i]] - dot(gradient[rows[i]], back);
        }
        const auto lambda = solve_dense(std::move(m), std::move(b));
        if (!lambda) {
            solution.reason = "the relations cannot be solved together";
            return solution;
        }
        std::vector<double> next = x0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            for (std::size_t k = 0; k < next.size(); ++k) {
                next[k] += gradient[rows[i]][k] * (*lambda)[i];
            }
        }
        double change = 0;
        for (std::size_t k = 0; k < x.size(); ++k) {
            change = std::max(change, std::abs(next[k] - x[k]));
        }
        x = std::move(next);
        if (change < 1e-12) {
            break;
        }
    }
    evaluate(constraints, x, value, gradient);
    if (std::ranges::any_of(value, [](double v) { return !(std::abs(v) < holds); })) {
        solution.reason = "the relations contradict each other";
        return solution;
    }
    for (std::size_t k = 0; k < x.size(); ++k) {
        solution.largest_move = std::max(solution.largest_move, std::abs(x[k] - x0[k]));
    }
    if (solution.largest_move > max_move + holds) {
        solution.reason = std::format("the relations need a move of {:.2f} px", solution.largest_move);
        return solution;
    }
    for (std::size_t p = 0; p < problem.primitives.size(); ++p) {
        std::vector<Vec2> anchors;
        for (std::size_t k = 0; k < problem.primitives[p].anchors.size(); ++k) {
            const std::size_t v = vertices.of({.primitive = p, .point = k});
            anchors.push_back({.x = x[2 * v], .y = x[(2 * v) + 1]});
        }
        solution.anchors.push_back(std::move(anchors));
    }
    solution.solved = true;
    return solution;
}

} // namespace sci::vector::detail
