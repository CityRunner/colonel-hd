// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/render/smoothing.hpp"

#include "sci/pic/geometry.hpp"
#include "sci/vector/raster_1x.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sci::render {

namespace {

using vector::picture_height;
using vector::picture_width;
using vector::Vec2;

struct Pixel {
    int x = 0;
    int y = 0;

    friend bool operator==(const Pixel&, const Pixel&) = default;
};

Pixel pixel_of(std::uint32_t index) {
    return {.x = static_cast<int>(index % picture_width), .y = static_cast<int>(index / picture_width)};
}

Pixel point_of(pic::Point p) {
    return {.x = p.x, .y = p.y};
}

int chebyshev(Pixel a, Pixel b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

Vec2 centre(Pixel p) {
    return {.x = p.x + 0.5, .y = p.y + 0.5};
}

double length(double x, double y) {
    return std::sqrt((x * x) + (y * y));
}

// The line of a trace primitive that draws into the visual plane, or null.
const pic::Line* visual_line(const pic::DrawTrace& trace, std::int32_t primitive) {
    if (primitive < 0 || static_cast<std::size_t>(primitive) >= trace.primitives.size()) {
        return nullptr;
    }
    const auto& traced = trace.primitives[static_cast<std::size_t>(primitive)];
    if ((traced.state.planes & pic::plane_visual) == 0) {
        return nullptr;
    }
    return std::get_if<pic::Line>(&traced.shape);
}

std::vector<std::uint32_t> pixels_of(const pic::Line& line) {
    const auto screen = [](pic::Point p) { return pic::ScreenPoint{.x = p.x, .y = p.y + pic::picture_top}; };
    return vector::line_pixels_1x(screen(line.from), screen(line.to));
}

// Per primitive: the visual pixels it owns at the end.
std::vector<int> owned_pixels(const pic::DrawTrace& trace) {
    std::vector<int> owned(trace.primitives.size(), 0);
    for (int y = 0; y < picture_height; ++y) {
        for (int x = 0; x < picture_width; ++x) {
            const std::int32_t owner = trace.visual_owner[pic::PictureMaps::index(x, y + pic::picture_top)];
            if (owner >= 0 && static_cast<std::size_t>(owner) < owned.size()) {
                ++owned[static_cast<std::size_t>(owner)];
            }
        }
    }
    return owned;
}

// --- Organic lines ---------------------------------------------------------

constexpr std::array organic_words{
    "tree",   "trees",   "branch",  "branches", "foliage", "leaf",    "leaves",  "shrub",   "bush",   "bushes",
    "hedge",  "vine",    "vines",   "root",     "roots",   "twig",    "twigs",   "sapling", "trunk",  "canopy",
    "oak",    "grass",   "lawn",    "weed",     "weeds",   "moss",    "mossy",   "flower",  "plant",  "rock",
    "rocks",  "boulder", "cloud",   "clouds",   "water",   "ripple",  "shore",   "shoreline", "lake", "pond",
    "swamp",  "bayou",   "river",   "mud",      "dirt",    "ground",  "terrain", "hill",    "mound",  "bank",
    "figure", "person",  "man",     "woman",    "coat",    "collar",  "lapel",   "clothing", "fold",  "hair",
};

bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// A 1x region (same colour pair, 4-connected) from a pixel; marks `mask`.
void fill_region(const pic::PictureMaps& maps, Pixel seed, std::vector<std::uint8_t>& mask) {
    if (seed.x < 0 || seed.x >= picture_width || seed.y < 0 || seed.y >= picture_height) {
        return;
    }
    const auto pair_at = [&](Pixel p) { return maps.pairs[pic::PictureMaps::index(p.x, p.y + pic::picture_top)]; };
    const pic::ColorPair pair = pair_at(seed);
    std::vector<Pixel> stack{seed};
    while (!stack.empty()) {
        const Pixel p = stack.back();
        stack.pop_back();
        if (p.x < 0 || p.x >= picture_width || p.y < 0 || p.y >= picture_height) {
            continue;
        }
        auto& m = mask[static_cast<std::size_t>((p.y * picture_width) + p.x)];
        if (m != 0 || pair_at(p) != pair) {
            continue;
        }
        m = 1;
        stack.push_back({.x = p.x + 1, .y = p.y});
        stack.push_back({.x = p.x - 1, .y = p.y});
        stack.push_back({.x = p.x, .y = p.y + 1});
        stack.push_back({.x = p.x, .y = p.y - 1});
    }
}

bool in_or_next_to(const std::vector<std::uint8_t>& mask, Pixel p) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int x = p.x + dx;
            const int y = p.y + dy;
            if (x >= 0 && x < picture_width && y >= 0 && y < picture_height &&
                mask[static_cast<std::size_t>((y * picture_width) + x)] != 0) {
                return true;
            }
        }
    }
    return false;
}

// The commands of scene-model primitives (picture, command) in organic
// objects, and the ids of every scene-model primitive.
struct SceneModelLines {
    std::set<std::pair<std::uint16_t, std::uint32_t>> organic;
    std::set<std::string> primitive_ids;
};

SceneModelLines scene_model_lines(std::span<const vector::PictureHints* const> hints) {
    SceneModelLines result;
    for (std::size_t p = 0; p < hints.size(); ++p) {
        if (hints[p] == nullptr) {
            continue;
        }
        for (const auto& object : hints[p]->objects) {
            std::vector<const vector::Hypothesis*> hypotheses{&object.main};
            for (const auto& alternative : object.alternatives) {
                hypotheses.push_back(&alternative);
            }
            for (const vector::Hypothesis* hypothesis : hypotheses) {
                for (const auto& part : hypothesis->parts) {
                    for (const auto& primitive : part.primitives) {
                        result.primitive_ids.insert(primitive.id);
                        if (!vector::is_organic(hypothesis->type)) {
                            continue;
                        }
                        for (const vector::LineRef command : primitive.commands) {
                            result.organic.emplace(static_cast<std::uint16_t>(p), command);
                        }
                    }
                }
            }
        }
    }
    return result;
}

// --- Step: joints across stacked parallel lines ----------------------------

enum class Axis : std::int8_t { None, Horizontal, Vertical };

// Horizontal or vertical, allowing one pixel of drift on a long line.
Axis axis_of(const pic::Line& line) {
    const int dx = std::abs(line.to.x - line.from.x);
    const int dy = std::abs(line.to.y - line.from.y);
    if ((dy == 0 && dx >= 1) || (dy == 1 && dx >= 6)) {
        return Axis::Horizontal;
    }
    if ((dx == 0 && dy >= 1) || (dx == 1 && dy >= 6)) {
        return Axis::Vertical;
    }
    return Axis::None;
}

Pixel nearest_end(const pic::Line& line, Pixel to) {
    const Pixel a = point_of(line.from);
    const Pixel b = point_of(line.to);
    return chebyshev(a, to) <= chebyshev(b, to) ? a : b;
}

struct StepHit {
    Pixel at;
    std::int32_t a = -1;
    std::int32_t b = -1;
    int weight = 0;
    int perpendicular = 0;
    int along = 0;
};

// The 1x colour pair of a picture pixel.
pic::ColorPair pair_at(const pic::PictureMaps& maps, Pixel p) {
    return maps.pairs[pic::PictureMaps::index(p.x, p.y + pic::picture_top)];
}

// Whether the HD change between a line's end and a target pixel shows: a
// pixel of the box between them (the end itself left out) shows another
// colour at 1x than the line, so the line's colour painted there changes
// what the player saw.
bool shows(const pic::PictureMaps& maps, const pic::DrawTrace& trace, std::int32_t line, Pixel end, Pixel target) {
    const pic::ColorPair own = trace.primitives[static_cast<std::size_t>(line)].state.pair;
    for (int y = std::min(end.y, target.y); y <= std::max(end.y, target.y); ++y) {
        for (int x = std::min(end.x, target.x); x <= std::max(end.x, target.x); ++x) {
            const Pixel p{.x = x, .y = y};
            if (p != end && x >= 0 && x < picture_width && y >= 0 && y < picture_height && pair_at(maps, p) != own) {
                return true;
            }
        }
    }
    return false;
}

// The pixel of a line beside an end pixel of a parallel line: in the same
// column (horizontal lines) or row (vertical), or the line's nearest end.
Pixel beside(const pic::Line& line, Pixel end, Axis axis) {
    for (const std::uint32_t index : pixels_of(line)) {
        const Pixel p = pixel_of(index);
        if ((axis == Axis::Horizontal && p.x == end.x) || (axis == Axis::Vertical && p.y == end.y)) {
            return p;
        }
    }
    return nearest_end(line, end);
}

std::optional<StepHit> step_hit(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                const vector::IntegrityChange& change) {
    if (change.kind != vector::IntegrityChangeKind::ClosedJoint &&
        change.kind != vector::IntegrityChangeKind::ClippedEnd) {
        return std::nullopt;
    }
    const pic::Line* a = visual_line(trace, change.primitive);
    const pic::Line* b = visual_line(trace, change.other);
    if (a == nullptr || b == nullptr) {
        return std::nullopt;
    }
    const Axis axis = axis_of(*a);
    if (axis == Axis::None || axis_of(*b) != axis) {
        return std::nullopt;
    }
    const Pixel at{.x = change.x, .y = change.y};
    const Pixel end_a = nearest_end(*a, at);
    const Pixel end_b = nearest_end(*b, end_a);
    const bool horizontal = axis == Axis::Horizontal;
    const int along = horizontal ? std::abs(end_a.x - end_b.x) : std::abs(end_a.y - end_b.y);
    // A joint of ends offset along the lines draws a diagonal connector; a
    // clip pulls an end onto the line beside it.
    const bool joint = change.kind == vector::IntegrityChangeKind::ClosedJoint;
    const Pixel target = joint ? end_b : beside(*b, end_a, axis);
    const int perpendicular = horizontal ? std::abs(end_a.y - target.y) : std::abs(end_a.x - target.x);
    if (perpendicular < 1 || perpendicular > 3 || (joint && (along < 1 || along > 4))) {
        return std::nullopt;
    }
    // Lines of at least three pixels (shorter ones are the raster bits of
    // organic shapes and textures); a joint only between lines of one
    // colour (one shape: a tread, a plank, a highlight).
    const auto span = [](const pic::Line& l) {
        return std::max(std::abs(l.to.x - l.from.x), std::abs(l.to.y - l.from.y)) + 1;
    };
    const auto pair_of = [&](std::int32_t l) { return trace.primitives[static_cast<std::size_t>(l)].state.pair; };
    if (std::min(span(*a), span(*b)) < 3 || (joint && pair_of(change.primitive) != pair_of(change.other))) {
        return std::nullopt;
    }
    if (!shows(maps, trace, change.primitive, end_a, target)) {
        return std::nullopt;
    }
    return StepHit{.at = end_a,
                   .a = change.primitive,
                   .b = change.other,
                   .weight = joint ? 100 : 60,
                   .perpendicular = perpendicular,
                   .along = along};
}

struct Box {
    int left = picture_width;
    int top = picture_height;
    int right = -1;
    int bottom = -1;

    void add(Pixel p) {
        left = std::min(left, p.x);
        top = std::min(top, p.y);
        right = std::max(right, p.x);
        bottom = std::max(bottom, p.y);
    }

    void add(const pic::Line& line) {
        add(point_of(line.from));
        add(point_of(line.to));
    }
};

void set_box(SmoothingSuspect& suspect, const Box& box) {
    suspect.left = std::clamp(box.left, 0, picture_width - 1);
    suspect.top = std::clamp(box.top, 0, picture_height - 1);
    suspect.right = std::clamp(box.right, 0, picture_width - 1);
    suspect.bottom = std::clamp(box.bottom, 0, picture_height - 1);
}

bool any_marked(const std::vector<std::uint8_t>& marks, std::span<const std::int32_t> lines) {
    return std::ranges::any_of(lines, [&](std::int32_t l) {
        return l >= 0 && static_cast<std::size_t>(l) < marks.size() && marks[static_cast<std::size_t>(l)] != 0;
    });
}

void sort_unique(std::vector<std::int32_t>& lines) {
    std::ranges::sort(lines);
    const auto [first, last] = std::ranges::unique(lines);
    lines.erase(first, last);
}

// Hits within 4 pixels of each other (transitively) form one suspect.
std::vector<SmoothingSuspect> step_suspects(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                            const vector::SceneStats& stats,
                                            const std::vector<std::uint8_t>& excluded) {
    std::vector<StepHit> hits;
    for (const auto& change : stats.integrity_changes) {
        if (auto hit = step_hit(maps, trace, change)) {
            const std::array lines{hit->a, hit->b};
            if (!any_marked(excluded, lines)) {
                hits.push_back(*hit);
            }
        }
    }
    // One hit per line end and other line (the pass may record a joint
    // twice, with two pieces of evidence).
    const auto key = [](const StepHit& h) { return std::tuple(h.at.y, h.at.x, h.a, h.b, -h.weight); };
    std::ranges::sort(hits, {}, key);
    const auto [first, last] = std::ranges::unique(
        hits, [](const StepHit& x, const StepHit& y) { return x.at == y.at && x.a == y.a && x.b == y.b; });
    hits.erase(first, last);
    std::vector<std::size_t> group(hits.size());
    for (std::size_t i = 0; i < hits.size(); ++i) {
        group[i] = i;
    }
    const auto find = [&](std::size_t i) {
        while (group[i] != i) {
            i = group[i];
        }
        return i;
    };
    for (std::size_t i = 0; i < hits.size(); ++i) {
        for (std::size_t j = i + 1; j < hits.size(); ++j) {
            if (chebyshev(hits[i].at, hits[j].at) <= 4) {
                const std::size_t a = find(i);
                const std::size_t b = find(j);
                group[std::max(a, b)] = std::min(a, b);
            }
        }
    }
    std::map<std::size_t, SmoothingSuspect> suspects;
    std::map<std::size_t, Box> boxes;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const std::size_t g = find(i);
        auto [it, inserted] = suspects.try_emplace(g);
        SmoothingSuspect& s = it->second;
        if (inserted) {
            s.kind = SmoothingKind::Step;
            s.x = hits[i].at.x;
            s.y = hits[i].at.y;
        }
        s.score += hits[i].weight;
        ++s.count;
        s.bend = std::max(s.bend, hits[i].perpendicular);
        s.deviation = std::max(s.deviation, hits[i].along);
        s.lines.push_back(hits[i].a);
        s.lines.push_back(hits[i].b);
        boxes[g].add({.x = hits[i].at.x - 2, .y = hits[i].at.y - 2});
        boxes[g].add({.x = hits[i].at.x + 2, .y = hits[i].at.y + 2});
    }
    std::vector<SmoothingSuspect> result;
    for (auto& [g, s] : suspects) {
        sort_unique(s.lines);
        // More stacked lines in one place: more clearly a staircase or a
        // stack of shelves or planks.
        s.score += 40 * std::max(0, static_cast<int>(s.lines.size()) - 2);
        set_box(s, boxes[g]);
        result.push_back(std::move(s));
    }
    return result;
}

// --- Corner: a fitted curve through a sharp vertex -------------------------

// How far a joint's connector passes from the corner of an L whose ends
// touch diagonally: about half a diagonal.
constexpr double cut_distance = 0.707;

// The fitted stroke of every line with fitted edges.
std::map<std::int32_t, const std::vector<Vec2>*> fitted_points(const vector::SceneStats& stats) {
    std::map<std::int32_t, const std::vector<Vec2>*> points;
    for (const auto& line : stats.fitted_strokes) {
        points.emplace(line.primitive, &line.points);
    }
    return points;
}

double distance_to_segment(Vec2 p, Vec2 a, Vec2 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double squared = (dx * dx) + (dy * dy);
    double t = 0;
    if (squared > 0) {
        t = std::clamp((((p.x - a.x) * dx) + ((p.y - a.y) * dy)) / squared, 0.0, 1.0);
    }
    return length(p.x - (a.x + (t * dx)), p.y - (a.y + (t * dy)));
}

double distance_to_polyline(Vec2 p, const std::vector<Vec2>& points) {
    if (points.size() == 1) {
        return length(p.x - points[0].x, p.y - points[0].y);
    }
    double best = 1e9;
    for (std::size_t i = 1; i < points.size(); ++i) {
        best = std::min(best, distance_to_segment(p, points[i - 1], points[i]));
    }
    return best;
}

// 1 - cos of the turn from direction u to direction v.
double bend_between(Pixel u, Pixel v) {
    const double lu = length(u.x, u.y);
    const double lv = length(v.x, v.y);
    if (lu == 0 || lv == 0) {
        return 0;
    }
    return 1 - ((static_cast<double>(u.x) * v.x) + (static_cast<double>(u.y) * v.y)) / (lu * lv);
}

Pixel minus(Pixel a, Pixel b) {
    return {.x = a.x - b.x, .y = a.y - b.y};
}

struct Ends {
    Pixel from;
    Pixel to;

    [[nodiscard]] Pixel other(Pixel end) const { return end == from ? to : from; }

    [[nodiscard]] int span() const { return chebyshev(from, to); }
};

// The bend at `end` of line `line` (into it) with another chain line that
// starts there, the largest if several; 0 if none.
double neighbour_bend(const std::map<std::int32_t, Ends>& lines, std::int32_t line, std::int32_t skip, Pixel end) {
    const Ends& own = lines.at(line);
    const Pixel into = minus(end, own.other(end));
    double bend = 0;
    for (const auto& [other, ends] : lines) {
        if (other == line || other == skip || (ends.from != end && ends.to != end)) {
            continue;
        }
        bend = std::max(bend, bend_between(into, minus(ends.other(end), end)));
    }
    return bend;
}

std::optional<SmoothingSuspect> corner_at(const std::map<std::int32_t, Ends>& lines, std::int32_t a, std::int32_t b,
                                          Pixel vertex,
                                          const std::map<std::int32_t, const std::vector<Vec2>*>& fitted) {
    const Ends& ea = lines.at(a);
    const Ends& eb = lines.at(b);
    if (std::min(ea.span(), eb.span()) < 2) {
        return std::nullopt;
    }
    const Pixel oa = ea.other(vertex);
    const Pixel ob = eb.other(vertex);
    const double bend = bend_between(minus(vertex, oa), minus(ob, vertex));
    constexpr double least_bend = 0.1; // About 26 degrees.
    if (bend < least_bend) {
        return std::nullopt;
    }
    const double around = std::max(neighbour_bend(lines, a, b, oa), neighbour_bend(lines, b, a, ob));
    if (bend < 2 * around) {
        return std::nullopt;
    }
    double distance = 1e9;
    for (const std::int32_t line : {a, b}) {
        if (const auto it = fitted.find(line); it != fitted.end() && !it->second->empty()) {
            distance = std::min(distance, distance_to_polyline(centre(vertex), *it->second));
        }
    }
    constexpr double least_distance = 0.2;
    if (distance > 1e8 || distance < least_distance) {
        return std::nullopt;
    }
    SmoothingSuspect s;
    s.kind = SmoothingKind::Corner;
    s.x = vertex.x;
    s.y = vertex.y;
    s.count = 1;
    s.bend = static_cast<int>(std::lround(bend * 1000));
    s.deviation = static_cast<int>(std::lround(distance * 1000));
    const int legs = std::min(std::min(ea.span(), eb.span()), 6);
    s.score = static_cast<int>(std::lround(bend * 100 * std::min(distance, 1.5) / cut_distance * legs / 3));
    s.lines = {std::min(a, b), std::max(a, b)};
    Box box;
    box.add(oa);
    box.add(ob);
    box.add(vertex);
    set_box(s, box);
    return s;
}

// A joint of two lines meeting at 60 to 120 degrees whose ends touch
// diagonally: the connector cuts the corner (a bevel) where it shows.
std::optional<SmoothingSuspect> bevel_at(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                         const vector::IntegrityChange& change) {
    const pic::Line* a = visual_line(trace, change.primitive);
    const pic::Line* b = visual_line(trace, change.other);
    if (change.kind != vector::IntegrityChangeKind::ClosedJoint || a == nullptr || b == nullptr) {
        return std::nullopt;
    }
    const Ends ea{.from = point_of(a->from), .to = point_of(a->to)};
    const Ends eb{.from = point_of(b->from), .to = point_of(b->to)};
    const Pixel end_a = nearest_end(*a, {.x = change.x, .y = change.y});
    const Pixel end_b = nearest_end(*b, end_a);
    if (std::abs(end_a.x - end_b.x) != 1 || std::abs(end_a.y - end_b.y) != 1 || std::min(ea.span(), eb.span()) < 3) {
        return std::nullopt;
    }
    const double bend = bend_between(minus(end_a, ea.other(end_a)), minus(eb.other(end_b), end_b));
    // 60 to 120 degrees: sharper turns back along a parallel line (a step).
    constexpr double least_bend = 0.5;
    constexpr double most_bend = 1.5;
    if (bend < least_bend || bend > most_bend) {
        return std::nullopt;
    }
    // One outline of one colour turning a corner, and both corner pixels
    // (inside and outside) of another colour at 1x: the connector shows.
    const pic::ColorPair own = trace.primitives[static_cast<std::size_t>(change.primitive)].state.pair;
    const Pixel outer{.x = end_a.x, .y = end_b.y};
    const Pixel inner{.x = end_b.x, .y = end_a.y};
    if (trace.primitives[static_cast<std::size_t>(change.other)].state.pair != own || pair_at(maps, outer) == own ||
        pair_at(maps, inner) == own) {
        return std::nullopt;
    }
    SmoothingSuspect s;
    s.kind = SmoothingKind::Corner;
    s.x = end_a.x;
    s.y = end_a.y;
    s.count = 1;
    s.bend = static_cast<int>(std::lround(bend * 1000));
    s.deviation = static_cast<int>(std::lround(cut_distance * 1000));
    const int legs = std::min(std::min(ea.span(), eb.span()), 6);
    s.score = static_cast<int>(std::lround(bend * 100 * legs / 3));
    s.lines = {std::min(change.primitive, change.other), std::max(change.primitive, change.other)};
    Box box;
    box.add(end_a);
    box.add(end_b);
    set_box(s, box);
    return s;
}

std::vector<SmoothingSuspect> corner_suspects(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                              const vector::SceneStats& stats,
                                              const std::vector<std::uint8_t>& excluded) {
    const auto fitted = fitted_points(stats);
    std::vector<SmoothingSuspect> result;
    std::set<std::pair<std::int32_t, std::int32_t>> bevelled;
    for (const auto& change : stats.integrity_changes) {
        if (auto s = bevel_at(maps, trace, change)) {
            if (!any_marked(excluded, s->lines) && bevelled.emplace(s->lines[0], s->lines[1]).second) {
                result.push_back(std::move(*s));
            }
        }
    }
    for (const auto& chain : stats.fitted_chains) {
        const bool curve = chain.kind == vector::FittedKind::Arc || chain.kind == vector::FittedKind::Spline ||
                           chain.kind == vector::FittedKind::Ellipse;
        if (!chain.hint.empty() || !curve) {
            continue;
        }
        std::map<std::int32_t, Ends> lines;
        for (const std::int32_t l : chain.lines) {
            if (const pic::Line* line = visual_line(trace, l); line != nullptr && line->from != line->to) {
                lines.emplace(l, Ends{.from = point_of(line->from), .to = point_of(line->to)});
            }
        }
        for (auto a = lines.begin(); a != lines.end(); ++a) {
            for (auto b = std::next(a); b != lines.end(); ++b) {
                for (const Pixel vertex : {a->second.from, a->second.to}) {
                    if (vertex != b->second.from && vertex != b->second.to) {
                        continue;
                    }
                    const std::array pair{a->first, b->first};
                    if (any_marked(excluded, pair)) {
                        continue;
                    }
                    if (auto s = corner_at(lines, a->first, b->first, vertex, fitted)) {
                        result.push_back(std::move(*s));
                    }
                }
            }
        }
    }
    return result;
}

// --- Curve: a regular curve drawn lumpy ------------------------------------

// A monotonic stand-in for the angle of a direction, in quarter turns
// (0 to 4), exact at multiples of 45 degrees: no trigonometry.
double diamond_angle(double x, double y) {
    if (y >= 0) {
        return x >= 0 ? y / (x + y) : 1 - (x / (-x + y));
    }
    return x < 0 ? 2 - (y / (-x - y)) : 3 + (x / (x - y));
}

// The turn from direction u to v in quarter turns, in (-2, 2].
double turn_between(Pixel u, Pixel v) {
    double t = diamond_angle(v.x, v.y) - diamond_angle(u.x, u.y);
    while (t > 2) {
        t -= 4;
    }
    while (t <= -2) {
        t += 4;
    }
    return t;
}

// A conic A u^2 + B uv + C v^2 + D u + E v + F = 0 with A + C = 1, in
// coordinates centred on `origin` and divided by `scale`.
struct Conic {
    double a = 0;
    double b = 0;
    double c = 0;
    double d = 0;
    double e = 0;
    double f = 0;
    Vec2 origin;
    double scale = 1;

    // The Sampson distance of a point, in pixels.
    [[nodiscard]] double distance(Vec2 p) const {
        const double u = (p.x - origin.x) / scale;
        const double v = (p.y - origin.y) / scale;
        const double q = (a * u * u) + (b * u * v) + (c * v * v) + (d * u) + (e * v) + f;
        const double gu = (2 * a * u) + (b * v) + d;
        const double gv = (b * u) + (2 * c * v) + e;
        const double g = length(gu, gv);
        return g == 0 ? 1e9 : std::abs(q) / g * scale;
    }
};

struct Weighted {
    Vec2 p;
    double weight = 1;
};

// Least squares with Gaussian elimination; nullopt if singular.
std::optional<std::array<double, 5>> solve(std::array<std::array<double, 6>, 5> m) {
    for (std::size_t col = 0; col < 5; ++col) {
        std::size_t pivot = col;
        for (std::size_t r = col + 1; r < 5; ++r) {
            if (std::abs(m[r][col]) > std::abs(m[pivot][col])) {
                pivot = r;
            }
        }
        if (std::abs(m[pivot][col]) < 1e-12) {
            return std::nullopt;
        }
        std::swap(m[pivot], m[col]);
        for (std::size_t r = 0; r < 5; ++r) {
            if (r == col) {
                continue;
            }
            const double factor = m[r][col] / m[col][col];
            for (std::size_t k = col; k < 6; ++k) {
                m[r][k] -= factor * m[col][k];
            }
        }
    }
    std::array<double, 5> x{};
    for (std::size_t r = 0; r < 5; ++r) {
        x[r] = m[r][5] / m[r][r];
    }
    return x;
}

std::optional<Conic> fit_conic(const std::vector<Weighted>& points, Vec2 origin, double scale) {
    std::array<std::array<double, 6>, 5> normal{};
    for (const Weighted& w : points) {
        const double u = (w.p.x - origin.x) / scale;
        const double v = (w.p.y - origin.y) / scale;
        const std::array<double, 5> row{(u * u) - (v * v), u * v, u, v, 1};
        const double target = -(v * v);
        for (std::size_t i = 0; i < 5; ++i) {
            for (std::size_t j = 0; j < 5; ++j) {
                normal[i][j] += w.weight * row[i] * row[j];
            }
            normal[i][5] += w.weight * row[i] * target;
        }
    }
    const auto x = solve(normal);
    if (!x) {
        return std::nullopt;
    }
    return Conic{.a = (*x)[0],
                 .b = (*x)[1],
                 .c = 1 - (*x)[0],
                 .d = (*x)[2],
                 .e = (*x)[3],
                 .f = (*x)[4],
                 .origin = origin,
                 .scale = scale};
}

// The semi-axes of an ellipse, in pixels, or nullopt if it is none.
std::optional<std::pair<double, double>> semi_axes(const Conic& k) {
    const double det = (4 * k.a * k.c) - (k.b * k.b);
    if (det <= 0) {
        return std::nullopt;
    }
    // The centre solves [2a b; b 2c] (u, v) = -(d, e).
    const double u0 = ((-k.d * 2 * k.c) + (k.b * k.e)) / det;
    const double v0 = ((-k.e * 2 * k.a) + (k.b * k.d)) / det;
    const double q0 = (k.a * u0 * u0) + (k.b * u0 * v0) + (k.c * v0 * v0) + (k.d * u0) + (k.e * v0) + k.f;
    const double mean = (k.a + k.c) / 2;
    const double spread = std::sqrt((((k.a - k.c) / 2) * ((k.a - k.c) / 2)) + ((k.b / 2) * (k.b / 2)));
    const double l1 = mean + spread;
    const double l2 = mean - spread;
    if (l1 <= 0 || l2 <= 0 || q0 >= 0) {
        return std::nullopt;
    }
    return std::pair{std::sqrt(-q0 / l2) * k.scale, std::sqrt(-q0 / l1) * k.scale};
}

// Lines drawn end to end, in order, and their end points (one more than
// the lines).
struct Path {
    std::vector<std::int32_t> lines;
    std::vector<Pixel> points;
};

// A line's end at a vertex.
struct LineEnd {
    std::int32_t line = -1;
    bool at_from = false;
};

// Paths through the visible lines: at a vertex, a line continues with the
// line there that turns least (at most 60 degrees), when each is the
// other's choice, whatever else meets there (bars, pins) and in whatever
// order the commands drew them.
std::vector<Path> paths(const pic::DrawTrace& trace, const std::vector<int>& owned) {
    std::map<std::pair<int, int>, std::vector<LineEnd>> at;
    for (std::size_t i = 0; i < trace.primitives.size(); ++i) {
        const pic::Line* line = visual_line(trace, static_cast<std::int32_t>(i));
        if (line == nullptr || line->from == line->to || owned[i] == 0) {
            continue;
        }
        at[{line->from.x, line->from.y}].push_back({.line = static_cast<std::int32_t>(i), .at_from = true});
        at[{line->to.x, line->to.y}].push_back({.line = static_cast<std::int32_t>(i), .at_from = false});
    }
    const auto ends = [&](std::int32_t l) {
        const pic::Line& line = *visual_line(trace, l);
        return std::pair{point_of(line.from), point_of(line.to)};
    };
    // Direction into the vertex along a line, and out of it.
    const auto into = [&](LineEnd e) {
        const auto [from, to] = ends(e.line);
        return e.at_from ? minus(from, to) : minus(to, from);
    };
    // The chosen continuation of every line end: key (line, at_from).
    std::map<std::pair<std::int32_t, bool>, LineEnd> choice;
    constexpr double most_bend = 0.5; // 60 degrees.
    for (const auto& [vertex, list] : at) {
        for (const LineEnd& e : list) {
            double best = most_bend;
            std::optional<LineEnd> pick;
            for (const LineEnd& f : list) {
                if (f.line == e.line) {
                    continue;
                }
                const Pixel out = minus(Pixel{} , into(f));
                if (const double bend = bend_between(into(e), out); bend <= best) {
                    best = bend;
                    pick = f;
                }
            }
            if (pick) {
                choice.emplace(std::pair{e.line, e.at_from}, *pick);
            }
        }
    }
    const auto next = [&](LineEnd e) -> std::optional<LineEnd> {
        const auto it = choice.find({e.line, e.at_from});
        if (it == choice.end()) {
            return std::nullopt;
        }
        const auto back = choice.find({it->second.line, it->second.at_from});
        if (back == choice.end() || back->second.line != e.line || back->second.at_from != e.at_from) {
            return std::nullopt;
        }
        return it->second;
    };
    std::vector<std::uint8_t> used(trace.primitives.size(), 0);
    std::vector<Path> result;
    // Walks from a line forwards (leaving it at `to` when !at_from... i.e.
    // entering at `entry`), appending until the path ends or closes.
    const auto walk = [&](LineEnd entry, Path& path) {
        std::optional<LineEnd> e = entry;
        while (e && used[static_cast<std::size_t>(e->line)] == 0) {
            used[static_cast<std::size_t>(e->line)] = 1;
            const auto [from, to] = ends(e->line);
            if (path.points.empty()) {
                path.points.push_back(e->at_from ? from : to);
            }
            path.lines.push_back(e->line);
            path.points.push_back(e->at_from ? to : from);
            e = next({.line = e->line, .at_from = !e->at_from});
        }
    };
    // Open paths first, from an end without a continuation; then cycles.
    for (const bool open : {true, false}) {
        for (const auto& [vertex, list] : at) {
            for (const LineEnd& e : list) {
                if (used[static_cast<std::size_t>(e.line)] != 0 || (open && next(e))) {
                    continue;
                }
                Path path;
                walk(e, path);
                if (path.lines.size() >= 4) {
                    result.push_back(std::move(path));
                }
            }
        }
    }
    return result;
}

// The HD stroke of a line: its fitted stroke, or the line between its end
// pixels' centres, in steps of at most a quarter pixel.
std::vector<Vec2> hd_points(const pic::Line& line, std::int32_t primitive,
                            const std::map<std::int32_t, const std::vector<Vec2>*>& fitted) {
    if (const auto it = fitted.find(primitive); it != fitted.end() && !it->second->empty()) {
        return *it->second;
    }
    const Vec2 a = centre(point_of(line.from));
    const Vec2 b = centre(point_of(line.to));
    const int steps = std::max(1, static_cast<int>(std::ceil(length(b.x - a.x, b.y - a.y) * 4)));
    std::vector<Vec2> points;
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        points.push_back({.x = a.x + ((b.x - a.x) * t), .y = a.y + ((b.y - a.y) * t)});
    }
    return points;
}

struct CurveLimits {
    static constexpr double max_vertex_turn = 0.67; // About 60 degrees: sharper is a corner.
    static constexpr double neutral_turn = 0.02;    // Straight on.
    static constexpr double least_total = 0.8;      // About 72 degrees.
    static constexpr double vertex_residual = 0.8;  // Pixels.
    static constexpr double pixel_residual = 1.5;   // Pixels.
    static constexpr double least_hd = 0.3;         // Pixels.
    static constexpr double largest_axis = 160;
    static constexpr double smallest_axis = 2;
    static constexpr double largest_ratio = 6;
};

std::optional<SmoothingSuspect> curve_of(const pic::DrawTrace& trace, std::span<const std::int32_t> run,
                                         std::span<const Pixel> vertices, double total_turn,
                                         const std::map<std::int32_t, const std::vector<Vec2>*>& fitted) {
    std::vector<Weighted> points;
    Vec2 origin;
    for (const std::int32_t l : run) {
        for (const std::uint32_t p : pixels_of(*visual_line(trace, l))) {
            points.push_back({.p = centre(pixel_of(p)), .weight = 1});
        }
    }
    // The vertices are the artist's points on the curve; the pixels of the
    // chords between them sag inwards, so they weigh a quarter of the
    // vertices together and only steady the fit.
    const double pixel_weight =
        static_cast<double>(vertices.size()) / (4 * static_cast<double>(std::max<std::size_t>(points.size(), 1)));
    for (Weighted& w : points) {
        w.weight = pixel_weight;
    }
    const std::size_t pixel_count = points.size();
    for (const Pixel v : vertices) {
        origin.x += centre(v).x / static_cast<double>(vertices.size());
        origin.y += centre(v).y / static_cast<double>(vertices.size());
        points.push_back({.p = centre(v), .weight = 1});
    }
    double scale = 1;
    for (const Pixel v : vertices) {
        scale = std::max(scale, length(centre(v).x - origin.x, centre(v).y - origin.y));
    }
    const auto conic = fit_conic(points, origin, scale);
    if (!conic) {
        return std::nullopt;
    }
    const auto axes = semi_axes(*conic);
    if (!axes || axes->first > CurveLimits::largest_axis || axes->second < CurveLimits::smallest_axis ||
        axes->first > CurveLimits::largest_ratio * axes->second) {
        return std::nullopt;
    }
    double vertex_residual = 0;
    for (const Pixel v : vertices) {
        vertex_residual = std::max(vertex_residual, conic->distance(centre(v)));
    }
    double pixel_residual = 0;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        pixel_residual = std::max(pixel_residual, conic->distance(points[i].p));
    }
    if (vertex_residual > CurveLimits::vertex_residual || pixel_residual > CurveLimits::pixel_residual) {
        return std::nullopt;
    }
    double hd = 0;
    Vec2 worst;
    Box box;
    for (const std::int32_t l : run) {
        const pic::Line& line = *visual_line(trace, l);
        box.add(line);
        for (const Vec2 p : hd_points(line, l, fitted)) {
            if (const double d = conic->distance(p); d > hd) {
                hd = d;
                worst = p;
            }
        }
    }
    if (hd < CurveLimits::least_hd) {
        return std::nullopt;
    }
    SmoothingSuspect s;
    s.kind = SmoothingKind::Curve;
    s.x = std::clamp(static_cast<int>(std::floor(worst.x)), 0, picture_width - 1);
    s.y = std::clamp(static_cast<int>(std::floor(worst.y)), 0, picture_height - 1);
    s.count = static_cast<int>(run.size());
    s.bend = static_cast<int>(std::lround(std::abs(total_turn) * 90));
    s.deviation = static_cast<int>(std::lround(hd * 1000));
    s.residual = static_cast<int>(std::lround(pixel_residual * 1000));
    s.score = static_cast<int>(std::lround(std::min(hd, 2.0) * 1000 * std::min(std::abs(total_turn), 2.0) / 2));
    s.lines.assign(run.begin(), run.end());
    sort_unique(s.lines);
    set_box(s, box);
    return s;
}

// Splits a path into runs that turn one way, at most about 60 degrees per
// vertex; checks each long enough run.
void curve_runs(const pic::DrawTrace& trace, const Path& path,
                const std::map<std::int32_t, const std::vector<Vec2>*>& fitted, std::vector<SmoothingSuspect>& out) {
    std::vector<Pixel> directions;
    for (std::size_t i = 1; i < path.points.size(); ++i) {
        directions.push_back(minus(path.points[i], path.points[i - 1]));
    }
    std::size_t start = 0; // First line of the run.
    int sign = 0;
    double total = 0;
    const auto close = [&](std::size_t end) { // One past the run's last line.
        if (end - start >= 4 && std::abs(total) >= CurveLimits::least_total) {
            const auto lines = std::span(path.lines).subspan(start, end - start);
            const auto vertices = std::span(path.points).subspan(start, end - start + 1);
            if (auto s = curve_of(trace, lines, vertices, total, fitted)) {
                out.push_back(std::move(*s));
            }
        }
    };
    for (std::size_t i = 1; i < directions.size(); ++i) {
        const double t = turn_between(directions[i - 1], directions[i]);
        const int s = std::abs(t) < CurveLimits::neutral_turn ? 0 : (t > 0 ? 1 : -1);
        if (std::abs(t) > CurveLimits::max_vertex_turn) {
            close(i);
            start = i;
            sign = 0;
            total = 0;
            continue;
        }
        if (s != 0 && sign != 0 && s != sign) {
            close(i);
            // The new run starts with the line before this vertex.
            start = i - 1;
            total = 0;
        }
        if (s != 0) {
            sign = s;
        }
        total += t;
    }
    close(directions.size());
}

std::vector<SmoothingSuspect> curve_suspects(const pic::DrawTrace& trace, const vector::SceneStats& stats,
                                             const std::vector<int>& owned,
                                             const std::vector<std::uint8_t>& excluded) {
    const auto fitted = fitted_points(stats);
    std::vector<SmoothingSuspect> result;
    for (const Path& path : paths(trace, owned)) {
        curve_runs(trace, path, fitted, result);
    }
    std::erase_if(result, [&](const SmoothingSuspect& s) { return any_marked(excluded, s.lines); });
    return result;
}

} // namespace

bool is_organic_label(std::string_view label) {
    std::string word;
    const auto check = [&] {
        const bool found = std::ranges::find(organic_words, std::string_view(word)) != organic_words.end();
        word.clear();
        return found;
    };
    for (const char c : label) {
        if (is_word_char(c)) {
            word.push_back(lower(c));
        } else if (!word.empty() && check()) {
            return true;
        }
    }
    return !word.empty() && check();
}

std::vector<std::uint8_t> organic_lines(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                        std::span<const vector::PictureHints* const> hints) {
    std::vector<std::uint8_t> organic(trace.primitives.size(), 0);
    const SceneModelLines model = scene_model_lines(hints);
    std::set<std::pair<std::uint16_t, std::uint32_t>> commands = model.organic;
    std::vector<std::uint8_t> mask(vector::picture_pixels, 0);
    for (std::size_t p = 0; p < hints.size(); ++p) {
        if (hints[p] == nullptr) {
            continue;
        }
        for (const auto& chain : hints[p]->chains) {
            if (is_organic_label(chain.label)) {
                for (const vector::LineRef line : chain.lines) {
                    commands.emplace(static_cast<std::uint16_t>(p), line);
                }
            }
        }
        for (const auto& region : hints[p]->regions) {
            if (is_organic_label(region.label)) {
                fill_region(maps, {.x = region.x, .y = region.y}, mask);
            }
        }
    }
    for (std::size_t i = 0; i < trace.primitives.size(); ++i) {
        const auto& traced = trace.primitives[i];
        if (commands.contains({traced.picture, traced.command})) {
            organic[i] = 1;
            continue;
        }
        const pic::Line* line = visual_line(trace, static_cast<std::int32_t>(i));
        if (line == nullptr) {
            continue;
        }
        const auto pixels = pixels_of(*line);
        const auto near = std::ranges::count_if(pixels, [&](std::uint32_t p) { return in_or_next_to(mask, pixel_of(p)); });
        if (!pixels.empty() && 2 * near >= static_cast<std::ptrdiff_t>(pixels.size())) {
            organic[i] = 1;
        }
    }
    return organic;
}

std::vector<SmoothingSuspect> find_smoothing_suspects(const pic::PictureMaps& maps, const pic::DrawTrace& trace,
                                                      const vector::SceneStats& visual,
                                                      std::span<const vector::PictureHints* const> hints) {
    // Excluded: organic lines, and lines the scene model steers.
    std::vector<std::uint8_t> excluded = organic_lines(maps, trace, hints);
    const SceneModelLines model = scene_model_lines(hints);
    for (const auto& chain : visual.fitted_chains) {
        if (!chain.hint.empty() && model.primitive_ids.contains(chain.hint)) {
            for (const std::int32_t l : chain.lines) {
                if (l >= 0 && static_cast<std::size_t>(l) < excluded.size()) {
                    excluded[static_cast<std::size_t>(l)] = 1;
                }
            }
        }
    }
    const std::vector<int> owned = owned_pixels(trace);
    std::vector<SmoothingSuspect> result = step_suspects(maps, trace, visual, excluded);
    for (auto& s : corner_suspects(maps, trace, visual, excluded)) {
        result.push_back(std::move(s));
    }
    for (auto& s : curve_suspects(trace, visual, owned, excluded)) {
        result.push_back(std::move(s));
    }
    std::ranges::stable_sort(result, [](const SmoothingSuspect& a, const SmoothingSuspect& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.kind != b.kind) {
            return a.kind < b.kind;
        }
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    return result;
}

} // namespace sci::render
