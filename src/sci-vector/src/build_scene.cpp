// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/vector/build_scene.hpp"

#include "curves.hpp"
#include "relations.hpp"
#include "scene_model.hpp"
#include "sci/core/rounding.hpp"
#include "sci/pic/geometry.hpp"
#include "sci/vector/raster_1x.hpp"
#include "shared_boundary.hpp"
#include "stroke_integrity.hpp"
#include "topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iterator>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace sci::vector {

namespace {

using core::IntPoint;
using core::RationalPoint;
using detail::TopoSegment;

constexpr int picture_top = pic::picture_top;

// Chains of lines up to this long (in original pixels, either axis) may be
// fitted by curves.
constexpr int max_curve_line = 12;

// How far a fitted curve may lie from its chain (spec section 6.3: about
// 0.5 to 1 pixel). At 0.5 the stroke along the curve, 1 pixel wide, still
// touches every pixel path 1 set for the lines.
constexpr double fit_tolerance = 0.5;

// Gaps up to this far (in doubled coordinates: 2 original pixels) are
// closed when the 1x line pixels touch (spec section 6.3).
constexpr std::int64_t gap_limit = 4;

// Curves a hint asks for may lie this far from their chain (the upper end
// of the spec's range); the line check decides whether the fit is kept.
constexpr double hint_tolerance = 1.0;

// Gap hints close gaps up to this far (doubled coordinates: 8 original
// pixels).
constexpr std::int64_t hint_gap_limit = 16;

[[nodiscard]] std::size_t picture_index(int x, int y) {
    return (static_cast<std::size_t>(y) * picture_width) + static_cast<std::size_t>(x);
}

// The plane's values and owners for the picture area.
struct PlaneView {
    std::vector<std::uint8_t> values = std::vector<std::uint8_t>(picture_pixels, 0);
    std::vector<std::int32_t> owners = std::vector<std::int32_t>(picture_pixels, pic::no_owner);
};

PlaneView view_plane(const pic::PictureMaps& maps, const pic::DrawTrace& trace, Plane plane) {
    PlaneView view;
    const std::size_t offset = static_cast<std::size_t>(picture_top) * picture_width;
    const auto& values = plane == Plane::Visual ? maps.pairs : maps.priority;
    const auto& owners = plane == Plane::Visual ? trace.visual_owner : trace.priority_owner;
    std::copy_n(values.begin() + static_cast<std::ptrdiff_t>(offset), picture_pixels, view.values.begin());
    std::copy_n(owners.begin() + static_cast<std::ptrdiff_t>(offset), picture_pixels, view.owners.begin());
    return view;
}

bool in_plane(const pic::TracedPrimitive& primitive, Plane plane) {
    const std::uint8_t bit = plane == Plane::Visual ? pic::plane_visual : pic::plane_priority;
    return (primitive.state.planes & bit) != 0;
}

std::uint8_t value_of(const pic::TracedPrimitive& primitive, Plane plane) {
    return plane == Plane::Visual ? primitive.state.pair : primitive.state.priority;
}

// A line as path 1 drew it: end points clipped to the screen.
struct DrawnLine {
    std::size_t primitive = 0;
    pic::ScreenPoint from;
    pic::ScreenPoint to;

    [[nodiscard]] bool is_dot() const { return from == to; }
};

// The doubled picture coordinates of a pixel centre given in screen
// coordinates.
IntPoint doubled(pic::ScreenPoint p) {
    return {.x = (2 * std::int64_t{p.x}) + 1, .y = (2 * std::int64_t{p.y - picture_top}) + 1};
}

Vec2 centre(pic::ScreenPoint p) {
    return {.x = p.x + 0.5, .y = (p.y - picture_top) + 0.5};
}

std::vector<DrawnLine> collect_lines(const pic::DrawTrace& trace, Plane plane) {
    std::vector<DrawnLine> lines;
    for (std::size_t i = 0; i < trace.primitives.size(); ++i) {
        const auto& primitive = trace.primitives[i];
        const auto* line = std::get_if<pic::Line>(&primitive.shape);
        if (line == nullptr || !in_plane(primitive, plane)) {
            continue;
        }
        const auto to_screen = [](pic::Point p) {
            return pic::clip_to_screen(pic::ScreenPoint{.x = p.x, .y = p.y + picture_top});
        };
        lines.push_back(DrawnLine{.primitive = i, .from = to_screen(line->from), .to = to_screen(line->to)});
    }
    return lines;
}

// Which lines set each screen pixel, as (pixel, line) pairs in order.
std::vector<std::pair<std::int32_t, std::int32_t>> index_line_pixels(const std::vector<DrawnLine>& lines) {
    std::vector<std::pair<std::int32_t, std::int32_t>> pixels;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        pic::for_each_line_pixel(lines[i].from, lines[i].to, [&](int x, int y) {
            pixels.emplace_back((y * pic::screen_width) + x, static_cast<std::int32_t>(i));
        });
    }
    std::ranges::stable_sort(pixels);
    const auto repeated = std::ranges::unique(pixels);
    pixels.erase(repeated.begin(), repeated.end());
    return pixels;
}

// How far from a line's end point its pixels are checked for contact
// with other lines (in original pixels).
constexpr int contact_radius = 2;

bool on_screen(int x, int y) {
    return x >= 0 && x < pic::screen_width && y >= 0 && y < pic::screen_height;
}

// Whether p lies within `limit` of the integer point e in both axes.
bool near(IntPoint e, const RationalPoint& p, std::int64_t limit_units = gap_limit) {
    const std::int64_t dx = core::checked_sub(p.x_num(), core::checked_mul(e.x, p.den()));
    const std::int64_t dy = core::checked_sub(p.y_num(), core::checked_mul(e.y, p.den()));
    const std::int64_t limit = core::checked_mul(limit_units, p.den());
    return dx >= -limit && dx <= limit && dy >= -limit && dy <= limit;
}

// How far p lies beyond the end point e of a line coming from `other`,
// scaled by the line's length: positive if p lies beyond e.
core::Rational beyond(IntPoint other, IntPoint e, const RationalPoint& p) {
    const IntPoint v = e - other;
    const std::int64_t dx = core::checked_sub(p.x_num(), core::checked_mul(e.x, p.den()));
    const std::int64_t dy = core::checked_sub(p.y_num(), core::checked_mul(e.y, p.den()));
    return {core::checked_add(core::checked_mul(dx, v.x), core::checked_mul(dy, v.y)), p.den()};
}

// A connector from e perpendicular to the target line, ending on it, if the
// foot lies on the target, within `limit` of e and not at e.
std::optional<TopoSegment> perpendicular_connector(IntPoint e, const TopoSegment& target, std::int64_t limit) {
    const auto foot = detail::perpendicular_foot(e, target);
    if (!foot || !near(e, *foot, limit) || *foot == RationalPoint(e)) {
        return std::nullopt;
    }
    const IntPoint d = target.b - target.a;
    const std::int64_t divisor = std::gcd(d.x, d.y);
    const IntPoint b{.x = e.x - (d.y / divisor), .y = e.y + (d.x / divisor)};
    if (std::abs(b.x) > detail::max_coordinate || std::abs(b.y) > detail::max_coordinate) {
        return std::nullopt;
    }
    return TopoSegment{.a = e, .b = b, .from = RationalPoint(e), .to = *foot};
}

// Closes the gaps where path 1's line pixels touch but the lines do not
// (fills spread to the four neighbours of a pixel only, so lines whose
// pixels touch, even diagonally, stop a fill). Preferred, in order: extend
// the line to the other line, join two end points, or add a perpendicular
// connector.
class GapCloser {
public:
    GapCloser(const std::vector<DrawnLine>& lines, std::vector<std::optional<TopoSegment>>& segments, SceneStats& stats)
        : lines_(&lines), segments_(&segments), stats_(&stats), pixels_(index_line_pixels(lines)) {}

    void close_all() {
        for (std::size_t i = 0; i < lines_->size(); ++i) {
            const DrawnLine& line = (*lines_)[i];
            if (line.is_dot()) {
                close_dot(i, line.from);
                continue;
            }
            close_end(i, line.to, line.from, false);
            close_end(i, line.from, line.to, true);
        }
    }

private:
    // The lines other than `line` whose pixels touch (share or neighbour)
    // the pixels of `line` within contact_radius of the end point: there a
    // fill stops between the two lines.
    [[nodiscard]] std::vector<std::int32_t> neighbours(std::size_t line, pic::ScreenPoint end) const {
        std::vector<pic::ScreenPoint> own;
        const DrawnLine& drawn = (*lines_)[line];
        pic::for_each_line_pixel(drawn.from, drawn.to, [&](int x, int y) {
            if (std::abs(x - end.x) <= contact_radius && std::abs(y - end.y) <= contact_radius) {
                own.push_back({.x = x, .y = y});
            }
        });
        std::vector<std::int32_t> found;
        for (const pic::ScreenPoint p : own) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!on_screen(p.x + dx, p.y + dy)) {
                        continue;
                    }
                    const int key = ((p.y + dy) * pic::screen_width) + p.x + dx;
                    auto it = std::ranges::lower_bound(pixels_, std::pair{key, std::int32_t{0}});
                    for (; it != pixels_.end() && it->first == key; ++it) {
                        if (std::cmp_not_equal(it->second, line)) {
                            found.push_back(it->second);
                        }
                    }
                }
            }
        }
        std::ranges::stable_sort(found);
        const auto repeated = std::ranges::unique(found);
        found.erase(repeated.begin(), repeated.end());
        return found;
    }

    void close_end(std::size_t line, pic::ScreenPoint end, pic::ScreenPoint other_end, bool is_from) {
        const IntPoint e = doubled(end);
        const IntPoint other = doubled(other_end);
        const TopoSegment own = detail::make_segment(other, e);
        std::optional<RationalPoint> extension;
        core::Rational extension_length{0};
        for (const std::int32_t index : neighbours(line, end)) {
            const DrawnLine& target_line = (*lines_)[static_cast<std::size_t>(index)];
            if (target_line.is_dot()) {
                connect_to_dot(e, own, doubled(target_line.from));
                continue;
            }
            const TopoSegment target = detail::make_segment(doubled(target_line.from), doubled(target_line.to));
            if (detail::segments_touch(own, target)) {
                continue;
            }
            const auto hit = detail::line_meets_segment(other, e, target);
            if (hit && near(e, *hit) && beyond(other, e, *hit) > core::Rational{0}) {
                const core::Rational length = beyond(other, e, *hit);
                if (!extension || length > extension_length) {
                    extension = hit;
                    extension_length = length;
                }
                continue;
            }
            if (join(e, target)) {
                continue;
            }
            perpendicular(e, target);
        }
        std::optional<TopoSegment>& segment = (*segments_)[line];
        if (extension && segment) {
            (is_from ? segment->from : segment->to) = *extension;
            ++stats_->extensions;
        }
        close_to_frame(e);
    }

    // Dots (lines of one pixel) have no segment; they take part through
    // connectors to the lines and dots next to them.
    void connect_to_dot(IntPoint e, const TopoSegment& own, IntPoint dot) {
        if (dot != e && detail::perpendicular_foot(dot, own) != RationalPoint(dot)) {
            connectors_.push_back(detail::make_segment(e, dot));
            ++stats_->joins;
        }
    }

    void close_dot(std::size_t line, pic::ScreenPoint at) {
        const IntPoint e = doubled(at);
        for (const std::int32_t index : neighbours(line, at)) {
            const DrawnLine& target_line = (*lines_)[static_cast<std::size_t>(index)];
            if (target_line.is_dot()) {
                // Each pair of dots once; dots on the same pixel need nothing.
                if (std::cmp_greater(index, line) && target_line.from != at) {
                    connectors_.push_back(detail::make_segment(e, doubled(target_line.from)));
                    ++stats_->joins;
                }
                continue;
            }
            // A line ending next to the dot connects itself to it.
            const auto adjacent = [&](pic::ScreenPoint p) {
                return std::abs(p.x - at.x) <= 1 && std::abs(p.y - at.y) <= 1;
            };
            if (adjacent(target_line.from) || adjacent(target_line.to)) {
                continue;
            }
            const TopoSegment target = detail::make_segment(doubled(target_line.from), doubled(target_line.to));
            if (detail::perpendicular_foot(e, target) == RationalPoint(e)) {
                continue; // The dot lies on the line.
            }
            if (!join(e, target)) {
                perpendicular(e, target);
            }
        }
        close_to_frame(e);
    }

    // A line pixel in the outermost row or column of the picture closes
    // the way along the picture's edge, which lies half a pixel further out.
    void close_to_frame(IntPoint e) {
        constexpr std::int64_t right = 2 * std::int64_t{picture_width};
        constexpr std::int64_t bottom = 2 * std::int64_t{picture_height};
        if (e.x == 1) {
            connectors_.push_back(detail::make_segment(e, {.x = 0, .y = e.y}));
        }
        if (e.x == right - 1) {
            connectors_.push_back(detail::make_segment(e, {.x = right, .y = e.y}));
        }
        if (e.y == 1) {
            connectors_.push_back(detail::make_segment(e, {.x = e.x, .y = 0}));
        }
        if (e.y == bottom - 1) {
            connectors_.push_back(detail::make_segment(e, {.x = e.x, .y = bottom}));
        }
    }

    bool join(IntPoint e, const TopoSegment& target) {
        std::optional<IntPoint> nearest;
        std::int64_t nearest_distance = 0;
        for (const IntPoint f : {target.a, target.b}) {
            const std::int64_t distance = std::max(std::abs(f.x - e.x), std::abs(f.y - e.y));
            if (distance > 0 && distance <= gap_limit && (!nearest || distance < nearest_distance)) {
                nearest = f;
                nearest_distance = distance;
            }
        }
        if (!nearest) {
            return false;
        }
        connectors_.push_back(detail::make_segment(e, *nearest));
        ++stats_->joins;
        return true;
    }

    void perpendicular(IntPoint e, const TopoSegment& target) {
        if (auto connector = perpendicular_connector(e, target, gap_limit)) {
            connectors_.push_back(*connector);
            ++stats_->perpendiculars;
        }
    }

public:
    [[nodiscard]] const std::vector<TopoSegment>& connectors() const { return connectors_; }

private:
    const std::vector<DrawnLine>* lines_;
    std::vector<std::optional<TopoSegment>>* segments_; // Empty for dots.
    std::vector<TopoSegment> connectors_;
    SceneStats* stats_;
    std::vector<std::pair<std::int32_t, std::int32_t>> pixels_;
};

std::vector<TopoSegment> picture_frame() {
    constexpr std::int64_t right = 2 * std::int64_t{picture_width};
    constexpr std::int64_t bottom = 2 * std::int64_t{picture_height};
    const std::array<IntPoint, 4> corners{
        IntPoint{.x = 0, .y = 0},
        IntPoint{.x = right, .y = 0},
        IntPoint{.x = right, .y = bottom},
        IntPoint{.x = 0, .y = bottom},
    };
    std::vector<TopoSegment> frame;
    frame.reserve(corners.size());
    for (std::size_t i = 0; i < corners.size(); ++i) {
        frame.push_back(detail::make_segment(corners[i], corners[(i + 1) % corners.size()]));
    }
    return frame;
}

// Pixels whose value differs from one of their eight neighbours.
std::vector<bool> edge_band(const std::vector<std::uint8_t>& values) {
    std::vector<bool> band(picture_pixels, false);
    for (int y = 0; y < picture_height; ++y) {
        for (int x = 0; x < picture_width; ++x) {
            const std::uint8_t value = values[picture_index(x, y)];
            for (int dy = -1; dy <= 1 && !band[picture_index(x, y)]; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < picture_width && ny >= 0 && ny < picture_height &&
                        values[picture_index(nx, ny)] != value) {
                        band[picture_index(x, y)] = true;
                        break;
                    }
                }
            }
        }
    }
    return band;
}

// Whether a pixel belongs to a region (filled, or left as cleared) rather
// than to a line or brush drawn over it.
bool is_region_pixel(const pic::DrawTrace& trace, std::int32_t owner) {
    return owner == pic::no_owner ||
           std::holds_alternative<pic::Fill>(trace.primitives[static_cast<std::size_t>(owner)].shape);
}

// A region pixel's contribution to its face's value.
struct Sample {
    std::int32_t face = 0;
    std::uint8_t value = 0;
    bool in_band = false;

    friend auto operator<=>(const Sample&, const Sample&) = default;
};

// The most frequent value of a face's samples (the smallest on a tie), and
// whether all samples outside the edge band have it.
std::pair<std::uint8_t, bool> decide_face(std::span<const Sample> samples) {
    std::uint8_t best = 0;
    std::size_t best_count = 0;
    for (std::size_t i = 0; i < samples.size();) {
        std::size_t j = i;
        while (j < samples.size() && samples[j].value == samples[i].value) {
            ++j;
        }
        if (j - i > best_count) {
            best = samples[i].value;
            best_count = j - i;
        }
        i = j;
    }
    const bool agree = std::ranges::none_of(samples, [&](const Sample& s) { return s.value != best && !s.in_band; });
    return {best, agree};
}

void count_face(const Face& face, bool has_samples, int pixels, SceneStats& stats) {
    if (pixels == 0) {
        return;
    }
    ++stats.faces;
    if (!has_samples) {
        ++stats.thin_faces;
    } else if (face.fill == FaceFill::Blocks) {
        ++stats.mixed_faces;
    } else {
        ++stats.uniform_faces;
    }
}

// The mixed faces' region pixels and bounding boxes, in face order.
std::vector<Fallback> list_fallbacks(const std::vector<Sample>& samples, const std::vector<std::size_t>& sample_pixels,
                                     const std::vector<Face>& faces, const std::vector<bool>& has_samples) {
    std::vector<Fallback> fallbacks;
    std::int32_t current = -1;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto face = static_cast<std::size_t>(samples[i].face);
        if (!has_samples[face] || faces[face].fill != FaceFill::Blocks) {
            continue;
        }
        const int x = static_cast<int>(sample_pixels[i] % picture_width);
        const int y = static_cast<int>(sample_pixels[i] / picture_width);
        if (samples[i].face != current) {
            current = samples[i].face;
            fallbacks.push_back(Fallback{.pixels = 0, .left = x, .top = y, .right = x, .bottom = y});
        }
        Fallback& fallback = fallbacks.back();
        ++fallback.pixels;
        fallback.left = std::min(fallback.left, x);
        fallback.right = std::max(fallback.right, x);
        fallback.top = std::min(fallback.top, y);
        fallback.bottom = std::max(fallback.bottom, y);
    }
    return fallbacks;
}

// Decides every face's value from the region pixels whose centres it
// contains. A face without any, or whose pixels disagree outside the edge
// band, falls back to blocks.
void colour_faces(const std::vector<std::int32_t>& labels, const PlaneView& view, const pic::DrawTrace& trace,
                  std::vector<Face>& faces, SceneStats& stats) {
    const std::vector<bool> band = edge_band(view.values);
    std::vector<Sample> samples;
    std::vector<std::size_t> pixel_of; // Per sample.
    std::vector<int> pixel_count(faces.size(), 0);
    for (std::size_t i = 0; i < picture_pixels; ++i) {
        const std::int32_t face = labels[i];
        if (face < 0) {
            continue;
        }
        ++pixel_count[static_cast<std::size_t>(face)];
        if (is_region_pixel(trace, view.owners[i])) {
            samples.push_back(Sample{.face = face, .value = view.values[i], .in_band = band[i]});
            pixel_of.push_back(i);
        }
    }
    // Sorted by face and value; the pixel of each sample goes along.
    std::vector<std::size_t> order(samples.size());
    // NOLINTNEXTLINE(modernize-use-ranges): the libc++ of Apple's SDK has no std::ranges::iota.
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, {}, [&](std::size_t i) { return samples[i]; });
    std::vector<Sample> sorted(samples.size());
    std::vector<std::size_t> sample_pixels(samples.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        sorted[i] = samples[order[i]];
        sample_pixels[i] = pixel_of[order[i]];
    }
    samples = std::move(sorted);
    std::vector<bool> has_samples(faces.size(), false);
    for (std::size_t start = 0; start < samples.size();) {
        const std::int32_t face = samples[start].face;
        std::size_t end = start;
        while (end < samples.size() && samples[end].face == face) {
            ++end;
        }
        const auto [value, agree] = decide_face(std::span(samples).subspan(start, end - start));
        faces[static_cast<std::size_t>(face)] =
            Face{.fill = agree ? FaceFill::Uniform : FaceFill::Blocks, .value = value};
        has_samples[static_cast<std::size_t>(face)] = true;
        start = end;
    }
    for (std::size_t f = 0; f < faces.size(); ++f) {
        if (!has_samples[f]) {
            faces[f].fill = FaceFill::Blocks;
        }
        count_face(faces[f], has_samples[f], pixel_count[f], stats);
    }
    stats.fallbacks = list_fallbacks(samples, sample_pixels, faces, has_samples);
    for (const Fallback& fallback : stats.fallbacks) {
        stats.fallback_pixels += fallback.pixels;
    }
}

// The value a fallback face shows in each pixel: the map's value, except
// in pixels of lines and brushes, which take the most frequent value of
// their region neighbours (the smallest on a tie).
std::vector<std::uint8_t> block_values(const PlaneView& view, const pic::DrawTrace& trace) {
    std::vector<std::uint8_t> blocks = view.values;
    for (int y = 0; y < picture_height; ++y) {
        for (int x = 0; x < picture_width; ++x) {
            if (is_region_pixel(trace, view.owners[picture_index(x, y)])) {
                continue;
            }
            std::array<int, 256> counts{};
            int best = -1;
            for (const auto [dx, dy] : {std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, -1}, std::pair{0, 1}}) {
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || nx >= picture_width || ny < 0 || ny >= picture_height ||
                    !is_region_pixel(trace, view.owners[picture_index(nx, ny)])) {
                    continue;
                }
                const std::uint8_t value = view.values[picture_index(nx, ny)];
                ++counts[value];
                if (best < 0 || counts[value] > counts[static_cast<std::size_t>(best)] ||
                    (counts[value] == counts[static_cast<std::size_t>(best)] && int{value} < best)) {
                    best = value;
                }
            }
            if (best >= 0) {
                blocks[picture_index(x, y)] = static_cast<std::uint8_t>(best);
            }
        }
    }
    return blocks;
}

// One scan edge, unless horizontal; `face_after` is the face of the
// half-edge running upwards.
void add_scan_edge(Vec2 from, Vec2 to, std::int32_t face_forward, std::int32_t face_backward,
                   std::vector<ScanEdge>& edges) {
    if (from.y == to.y) {
        return;
    }
    const bool down = to.y > from.y;
    const Vec2 top = down ? from : to;
    const Vec2 bottom = down ? to : from;
    if (bottom.y <= 0 || top.y >= picture_height) {
        return;
    }
    edges.push_back(ScanEdge{.top = top, .bottom = bottom, .face_after = down ? face_backward : face_forward});
}

// The scan edges of an edge whose course the shared boundary set (from
// half-edge 2i's origin), if it did.
bool add_override(const detail::Arrangement& arrangement, std::size_t edge,
                  const std::vector<std::optional<std::vector<Vec2>>>* overrides, std::vector<ScanEdge>& edges) {
    if (overrides == nullptr) {
        return false;
    }
    const auto& found = (*overrides)[edge];
    if (!found) {
        return false;
    }
    const auto& course = *found;
    const std::int32_t forward = arrangement.half_edges[2 * edge].face;
    const std::int32_t backward = arrangement.half_edges[(2 * edge) + 1].face;
    for (std::size_t k = 0; k + 1 < course.size(); ++k) {
        add_scan_edge(course[k], course[k + 1], forward, backward, edges);
    }
    return true;
}

// The faces' boundaries: the arrangement's edges, the fitted pieces where
// a fit replaced them, and the courses the shared boundary set (spec
// section 6.6) where it moved them.
std::vector<ScanEdge> scan_edges(const detail::Arrangement& arrangement, const std::vector<detail::FittedChain>& fitted,
                                 const std::vector<std::optional<std::vector<Vec2>>>* overrides) {
    std::vector<bool> replaced(arrangement.sources.size(), false);
    std::vector<ScanEdge> edges;
    for (const detail::FittedChain& chain : fitted) {
        for (std::size_t i = 0; i < chain.edges.size(); ++i) {
            replaced[static_cast<std::size_t>(chain.edges[i])] = true;
            if (add_override(arrangement, static_cast<std::size_t>(chain.edges[i]), overrides, edges)) {
                continue;
            }
            const detail::HalfEdge& along = arrangement.half_edges[static_cast<std::size_t>(chain.half_edges[i])];
            const std::int32_t back = arrangement.half_edges[static_cast<std::size_t>(along.twin)].face;
            const auto& piece = chain.pieces[i];
            for (std::size_t k = 0; k + 1 < piece.size(); ++k) {
                add_scan_edge(piece[k], piece[k + 1], along.face, back, edges);
            }
        }
    }
    for (std::size_t edge = 0; edge < replaced.size(); ++edge) {
        if (replaced[edge] || add_override(arrangement, edge, overrides, edges)) {
            continue;
        }
        const detail::HalfEdge& forward = arrangement.half_edges[2 * edge];
        const detail::HalfEdge& backward = arrangement.half_edges[(2 * edge) + 1];
        add_scan_edge(detail::to_picture(arrangement.vertices[static_cast<std::size_t>(forward.origin)]),
                      detail::to_picture(arrangement.vertices[static_cast<std::size_t>(backward.origin)]), forward.face,
                      backward.face, edges);
    }
    return edges;
}

// The pixels a primitive owns at the end, with their values.
Blocks owned_blocks(const PlaneView& view, std::int32_t primitive) {
    Blocks blocks{.source = primitive, .pixels = {}, .values = {}};
    for (std::size_t i = 0; i < picture_pixels; ++i) {
        if (view.owners[i] == primitive) {
            blocks.pixels.push_back(static_cast<std::uint32_t>(i));
            blocks.values.push_back(view.values[i]);
        }
    }
    return blocks;
}

std::optional<Layer> brush_layer(const pic::Pattern& pattern, std::uint8_t value, std::int32_t source) {
    const int size = pic::pattern_size(pattern.code);
    const pic::PixelBox box = pic::pattern_box(pattern.at.x, pattern.at.y, size);
    const double left = box.left;
    const double right = std::min(box.right, pic::screen_width);
    const double top = box.top - picture_top;
    const double bottom = box.bottom - picture_top;
    if (pic::pattern_is_rectangle(pattern.code)) {
        return Box{.source = source, .left = left, .top = top, .right = right, .bottom = bottom, .value = value};
    }
    // The size 1 circle draws a plus of 5 pixels in the left 3 x 3 pixels
    // of its 4 x 3 box: at 1x a small diamond (picture 45's wallpaper
    // ornament). It is that diamond, corners at the plus's outer pixel
    // edges, not the larger oval inscribed in the box.
    if (size == 1) {
        return Ellipse{
            .source = source,
            .centre = {.x = left + 1.5, .y = top + 1.5},
            .radius_x = 1.5,
            .radius_y = 1.5,
            .value = value,
            .diamond = true,
        };
    }
    return Ellipse{
        .source = source,
        .centre = {.x = (left + right) / 2, .y = (top + bottom) / 2},
        .radius_x = (right - left) / 2,
        .radius_y = (bottom - top) / 2,
        .value = value,
        .diamond = false,
    };
}

// How many pixels each primitive owns at the end.
std::vector<int> owned_pixels(const pic::DrawTrace& trace, const PlaneView& view) {
    std::vector<int> owned(trace.primitives.size(), 0);
    for (const std::int32_t owner : view.owners) {
        if (owner != pic::no_owner) {
            ++owned[static_cast<std::size_t>(owner)];
        }
    }
    return owned;
}

// The hints that refer to lines of this plane, with their lines as line
// indices.
struct HintChain {
    int picture = 0;
    const ChainHint* hint = nullptr;
    std::vector<std::size_t> lines;
};

struct HintGap {
    int picture = 0;
    const GapHint* hint = nullptr;
    std::size_t from = 0;
    std::size_t to = 0;
};

struct PlaneHints {
    std::vector<HintChain> chains;
    std::vector<HintGap> gaps;
};

std::string_view kind_name(HintPrimitive primitive) {
    switch (primitive) {
    case HintPrimitive::Line:
        return "a line";
    case HintPrimitive::Arc:
        return "arcs";
    case HintPrimitive::Spline:
        return "a spline";
    case HintPrimitive::Ellipse:
        return "an ellipse";
    }
    return "a spline";
}

detail::CurveKind curve_kind(HintPrimitive primitive) {
    switch (primitive) {
    case HintPrimitive::Line:
        return detail::CurveKind::Line;
    case HintPrimitive::Arc:
        return detail::CurveKind::Arc;
    case HintPrimitive::Spline:
        return detail::CurveKind::Spline;
    case HintPrimitive::Ellipse:
        return detail::CurveKind::Ellipse;
    }
    return detail::CurveKind::Spline;
}

// Finds the lines that hints refer to by picture and command.
class HintLines {
public:
    HintLines(const pic::DrawTrace& trace, const std::vector<DrawnLine>& lines) {
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const auto& primitive = trace.primitives[lines[i].primitive];
            in_plane_.emplace_back(key(primitive.picture, primitive.command), i);
        }
        for (const auto& primitive : trace.primitives) {
            if (std::holds_alternative<pic::Line>(primitive.shape)) {
                drawn_.push_back(key(primitive.picture, primitive.command));
            }
        }
        std::ranges::stable_sort(in_plane_);
        std::ranges::stable_sort(drawn_);
    }

    // The index of the line in this plane.
    [[nodiscard]] std::optional<std::size_t> line(std::size_t picture, LineRef command) const {
        const std::uint64_t k = key(picture, command);
        const auto it = std::ranges::lower_bound(in_plane_, std::pair{k, std::size_t{0}});
        if (it == in_plane_.end() || it->first != k) {
            return std::nullopt;
        }
        return it->second;
    }

    // Whether the command draws a line, in any plane.
    [[nodiscard]] bool drawn(std::size_t picture, LineRef command) const {
        return std::ranges::binary_search(drawn_, key(picture, command));
    }

private:
    static std::uint64_t key(std::size_t picture, LineRef command) {
        return (static_cast<std::uint64_t>(picture) << 32U) | command;
    }

    std::vector<std::pair<std::uint64_t, std::size_t>> in_plane_;
    std::vector<std::uint64_t> drawn_;
};

void reject_hint(SceneStats& stats, int picture, const std::string& id, std::string reason) {
    stats.rejected_hints.push_back(HintOutcome{.picture = picture, .id = id, .detail = std::move(reason)});
}

// A chain hint's lines in this plane; nothing if it has fewer than two
// here, or a command that draws no line at all.
std::optional<HintChain> resolve_chain(const HintLines& find, std::size_t p, const PictureHints& picture,
                                       const ChainHint& chain, SceneStats& stats) {
    const auto missing = std::ranges::find_if(chain.lines, [&](LineRef ref) { return !find.drawn(p, ref); });
    if (missing != chain.lines.end()) {
        reject_hint(stats, picture.picture, chain.id, std::format("command {} draws no line", *missing));
        return std::nullopt;
    }
    HintChain found{.picture = picture.picture, .hint = &chain, .lines = {}};
    for (const LineRef ref : chain.lines) {
        if (const auto line = find.line(p, ref)) {
            found.lines.push_back(*line);
        }
    }
    if (found.lines.size() < 2) {
        return std::nullopt;
    }
    return found;
}

std::optional<HintGap> resolve_gap(const HintLines& find, std::size_t p, const PictureHints& picture,
                                   const GapHint& gap, SceneStats& stats) {
    for (const LineRef ref : {gap.from, gap.to}) {
        if (!find.drawn(p, ref)) {
            reject_hint(stats, picture.picture, gap.id, std::format("command {} draws no line", ref));
            return std::nullopt;
        }
    }
    const auto from = find.line(p, gap.from);
    const auto to = find.line(p, gap.to);
    if (!from || !to) {
        return std::nullopt;
    }
    return HintGap{.picture = picture.picture, .hint = &gap, .from = *from, .to = *to};
}

PlaneHints resolve_hints(const pic::DrawTrace& trace, const std::vector<DrawnLine>& lines,
                         std::span<const PictureHints* const> hints, SceneStats& stats) {
    const HintLines find(trace, lines);
    PlaneHints resolved;
    for (std::size_t p = 0; p < hints.size() && p < trace.pictures; ++p) {
        if (hints[p] == nullptr) {
            continue;
        }
        for (const ChainHint& chain : hints[p]->chains) {
            if (auto found = resolve_chain(find, p, *hints[p], chain, stats)) {
                resolved.chains.push_back(std::move(*found));
            }
        }
        for (const GapHint& gap : hints[p]->gaps) {
            if (auto found = resolve_gap(find, p, *hints[p], gap, stats)) {
                resolved.gaps.push_back(*found);
            }
        }
    }
    return resolved;
}

// The connector a gap hint asks for: from the line's end to the nearest
// point of the other line, if that is within hint_gap_limit and not
// already touching.
std::optional<TopoSegment> gap_connector(const HintGap& gap, const std::vector<DrawnLine>& lines) {
    const DrawnLine& from = lines[gap.from];
    const DrawnLine& to = lines[gap.to];
    const IntPoint e = doubled(gap.hint->end == LineEnd::Start ? from.from : from.to);
    const auto chebyshev = [&](IntPoint p) { return std::max(std::abs(p.x - e.x), std::abs(p.y - e.y)); };
    if (!to.is_dot()) {
        const TopoSegment target = detail::make_segment(doubled(to.from), doubled(to.to));
        if (detail::perpendicular_foot(e, target) == RationalPoint(e)) {
            return std::nullopt;
        }
        if (auto connector = perpendicular_connector(e, target, hint_gap_limit)) {
            return connector;
        }
    }
    const IntPoint a = doubled(to.from);
    const IntPoint b = doubled(to.to);
    const IntPoint nearest = chebyshev(a) <= chebyshev(b) ? a : b;
    if (nearest == e || chebyshev(nearest) > hint_gap_limit) {
        return std::nullopt;
    }
    return detail::make_segment(e, nearest);
}

// The faces of a topology, coloured, with the face counts of the stats.
struct Partition {
    detail::Arrangement arrangement;
    std::vector<Face> faces;
    SceneStats stats;
};

// A segment of doubled coordinates in units scale times finer, exactly.
TopoSegment scaled(const TopoSegment& segment, std::int64_t scale) {
    if (scale == 1) {
        return segment;
    }
    const auto point = [&](IntPoint p) {
        return IntPoint{.x = core::checked_mul(p.x, scale), .y = core::checked_mul(p.y, scale)};
    };
    const auto rational = [&](const RationalPoint& p) {
        return RationalPoint(core::checked_mul(p.x_num(), scale), core::checked_mul(p.y_num(), scale), p.den());
    };
    return TopoSegment{
        .a = point(segment.a),
        .b = point(segment.b),
        .from = rational(segment.from),
        .to = rational(segment.to),
        .source = segment.source,
    };
}

// Partitions the topologies of one plane: segments in units of
// 1 / (2 * scale) picture pixels (docs/hd-pipeline.md, "Curved shapes in
// the arrangement"). The fast analysis keeps the last partitions it made
// and gives a copy where the same segments come again at the same scale (a
// shape entered is partitioned to be checked, and again at the start of
// the next round; docs/hd-pipeline.md, "Room entry"); the reference makes
// every one.
class Partitioner {
public:
    Partitioner(const PlaneView& view, const pic::DrawTrace& trace, Analysis analysis)
        : view_(&view), trace_(&trace), analysis_(analysis) {}

    Partition operator()(std::span<const TopoSegment> topology, std::int64_t scale = 1) {
        if (analysis_ == Analysis::Fast) {
            for (const Made& made : made_) {
                if (made.scale == scale && std::ranges::equal(made.topology, topology)) {
                    return made.partition;
                }
            }
        }
        detail::Arrangement arrangement = detail::build_arrangement(topology, scale, analysis_);
        Partition result{.arrangement = std::move(arrangement), .faces = {}, .stats = {}};
        result.faces.resize(static_cast<std::size_t>(result.arrangement.face_count));
        const std::vector<std::int32_t> labels =
            detail::label_pixel_centres(result.arrangement, picture_width, picture_height);
        colour_faces(labels, *view_, *trace_, result.faces, result.stats);
        if (analysis_ == Analysis::Fast) {
            if (made_.size() == kept) {
                made_.erase(made_.begin());
            }
            made_.push_back(Made{
                .topology = std::vector<TopoSegment>(topology.begin(), topology.end()),
                .scale = scale,
                .partition = result,
            });
        }
        return result;
    }

    [[nodiscard]] Analysis analysis() const { return analysis_; }

private:
    static constexpr std::size_t kept = 4;

    struct Made {
        std::vector<TopoSegment> topology;
        std::int64_t scale = 1;
        Partition partition;
    };

    const PlaneView* view_;
    const pic::DrawTrace* trace_;
    Analysis analysis_;
    std::vector<Made> made_;
};

// Whether a partition has no more mixed faces and 1x blocks than another
// (the rule gap hints must keep).
bool no_worse(const Partition& candidate, const Partition& base) {
    return candidate.stats.mixed_faces <= base.stats.mixed_faces &&
           candidate.stats.fallback_pixels <= base.stats.fallback_pixels;
}

// Partitions the topology, closing the gaps that hints ask for where that
// keeps the rule: all at once if they keep it together, else one by one.
Partition close_hinted_gaps(std::vector<TopoSegment> topology, std::int64_t scale, const PlaneHints& hints,
                            const std::vector<DrawnLine>& lines, Partitioner& partition, SceneStats& stats) {
    Partition base = partition(topology, scale);
    std::vector<std::pair<const HintGap*, TopoSegment>> candidates;
    for (const HintGap& gap : hints.gaps) {
        if (auto connector = gap_connector(gap, lines)) {
            candidates.emplace_back(&gap, scaled(*connector, scale));
        } else {
            reject_hint(stats, gap.picture, gap.hint->id, "the lines already touch, or are too far apart");
        }
    }
    if (candidates.empty()) {
        return base;
    }
    std::vector<TopoSegment> all = topology;
    for (const auto& candidate : candidates) {
        all.push_back(candidate.second);
    }
    if (Partition closed = partition(all, scale); no_worse(closed, base)) {
        stats.hint_gaps += static_cast<int>(candidates.size());
        for (const auto& candidate : candidates) {
            stats.applied_hints.push_back(HintOutcome{
                .picture = candidate.first->picture,
                .id = candidate.first->hint->id,
                .detail = "gap closed",
            });
        }
        return closed;
    }
    for (const auto& [gap, connector] : candidates) {
        topology.push_back(connector);
        if (Partition closed = partition(topology, scale); no_worse(closed, base)) {
            base = std::move(closed);
            ++stats.hint_gaps;
            stats.applied_hints.push_back(
                HintOutcome{.picture = gap->picture, .id = gap->hint->id, .detail = "gap closed"});
        } else {
            topology.pop_back();
            reject_hint(stats, gap->picture, gap->hint->id, "closing it adds mixed faces or 1x blocks");
        }
    }
    return base;
}

void copy_face_stats(const SceneStats& from, SceneStats& to) {
    to.faces = from.faces;
    to.uniform_faces = from.uniform_faces;
    to.thin_faces = from.thin_faces;
    to.mixed_faces = from.mixed_faces;
    to.fallback_pixels = from.fallback_pixels;
    to.fallbacks = from.fallbacks;
}

// Whether a stroke along the points, 1 pixel wide, covers one of the
// pixel's 16 x 16 samples (the acceptance check's line rule, spec section
// 6.4).
bool stroke_touches(const std::vector<Vec2>& points, int x, int y) {
    constexpr int samples = 16;
    constexpr double radius = 0.5;
    const auto within = [&](Vec2 p) {
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
            const Vec2 a = points[i];
            const Vec2 d{.x = points[i + 1].x - a.x, .y = points[i + 1].y - a.y};
            const double length_squared = (d.x * d.x) + (d.y * d.y);
            const double t = length_squared == 0
                                 ? 0
                                 : std::clamp((((p.x - a.x) * d.x) + ((p.y - a.y) * d.y)) / length_squared, 0.0, 1.0);
            const double ex = p.x - a.x - (t * d.x);
            const double ey = p.y - a.y - (t * d.y);
            if ((ex * ex) + (ey * ey) <= radius * radius) {
                return true;
            }
        }
        return false;
    };
    if (within({.x = x + 0.5, .y = y + 0.5})) {
        return true;
    }
    for (int sy = 0; sy < samples; ++sy) {
        for (int sx = 0; sx < samples; ++sx) {
            if (within({.x = x + ((sx + 0.5) / samples), .y = y + ((sy + 0.5) / samples)})) {
                return true;
            }
        }
    }
    return false;
}

// Replaces the chains of short visible lines that imitate curves.
// Lines of a chain hint (hint_of_line, the hint's index, or -1) take part
// whether short or not; failed_hints lists the hints whose fit failed.
std::vector<detail::FittedChain> fit_curves(const detail::Arrangement& arrangement, const std::vector<DrawnLine>& lines,
                                            const std::vector<std::optional<TopoSegment>>& segments,
                                            const std::vector<int>& owned, std::span<const std::int32_t> hint_of_line,
                                            std::span<const detail::CurveKind> hint_kinds,
                                            std::vector<std::int32_t>& failed_hints, SceneStats& stats) {
    std::vector<detail::ChainLine> chain_lines(lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const DrawnLine& line = lines[i];
        const IntPoint a = doubled(line.from);
        const IntPoint b = doubled(line.to);
        // Lines extended to close a gap end on another line: they are not
        // part of a curve there.
        const std::optional<TopoSegment>& segment = segments[i];
        const bool extended =
            !segment.has_value() || segment->from != RationalPoint(a) || segment->to != RationalPoint(b);
        const bool short_line =
            std::abs(line.from.x - line.to.x) <= max_curve_line && std::abs(line.from.y - line.to.y) <= max_curve_line;
        const bool visible = !line.is_dot() && owned[lines[i].primitive] > 0;
        chain_lines[i] = detail::ChainLine{
            .usable = visible && !extended && short_line,
            .hint = visible ? hint_of_line[i] : -1,
        };
    }
    detail::CurveStats curve_stats;
    auto fitted = detail::fit_chains(arrangement, chain_lines, fit_tolerance, hint_kinds, hint_tolerance, curve_stats);
    failed_hints = std::move(curve_stats.failed_hints);
    stats.straightened = curve_stats.lines;
    stats.arcs = curve_stats.arcs;
    stats.splines = curve_stats.splines;
    stats.ellipses = curve_stats.ellipses;
    std::vector<bool> fitted_line(lines.size(), false);
    for (const detail::FittedChain& chain : fitted) {
        for (const std::int32_t edge : chain.edges) {
            fitted_line[static_cast<std::size_t>(arrangement.sources[static_cast<std::size_t>(edge)])] = true;
        }
    }
    stats.fitted_lines = static_cast<int>(std::ranges::count(fitted_line, true));
    stats.rejected_fits = curve_stats.rejected;
    return fitted;
}

// The points of an edge of a fitted line, from the end nearer the line's
// start: its fitted piece, or the straight edge.
std::vector<Vec2> edge_points(const detail::Arrangement& arrangement, std::size_t edge, const std::vector<Vec2>* piece,
                              bool piece_reversed, const DrawnLine& line) {
    std::vector<Vec2> points;
    if (piece != nullptr) {
        points = *piece;
        if (piece_reversed) {
            std::ranges::reverse(points);
        }
    } else {
        const auto vertex = [&](std::size_t half_edge) {
            return detail::to_picture(
                arrangement.vertices[static_cast<std::size_t>(arrangement.half_edges[half_edge].origin)]);
        };
        points = {vertex(2 * edge), vertex((2 * edge) + 1)};
    }
    const Vec2 from = centre(line.from);
    const Vec2 to = centre(line.to);
    const auto along = [&](Vec2 p) { return ((p.x - from.x) * (to.x - from.x)) + ((p.y - from.y) * (to.y - from.y)); };
    if (along(points.back()) < along(points.front())) {
        std::ranges::reverse(points);
    }
    return points;
}

// A line's stroke from its parts in order along the line. Parts left out
// (edges shared with overlapping segments) are straight pieces of the line:
// joining the parts restores them.
std::vector<Vec2> join_parts(std::vector<std::vector<Vec2>> parts, const DrawnLine& line) {
    const Vec2 from = centre(line.from);
    const Vec2 to = centre(line.to);
    const auto along = [&](const std::vector<Vec2>& part) {
        return ((part.front().x - from.x) * (to.x - from.x)) + ((part.front().y - from.y) * (to.y - from.y));
    };
    std::ranges::stable_sort(parts, {}, along);
    std::vector<Vec2> points{from};
    for (const auto& part : parts) {
        for (const Vec2 p : part) {
            if (points.back() != p) {
                points.push_back(p);
            }
        }
    }
    if (points.back() != to) {
        points.push_back(to);
    }
    return points;
}

// The stroke of every line with fitted edges: the pieces of its edges
// (fitted curves, or straight where not fitted), from the line's start to
// its end. Empty for lines without fitted edges.
std::vector<std::vector<Vec2>> stroke_curves(const detail::Arrangement& arrangement,
                                             const std::vector<detail::FittedChain>& fitted,
                                             const std::vector<DrawnLine>& lines) {
    // The fitted piece of every fitted edge, and whether the chain runs
    // against the edge's own direction (from half-edge 2i's origin).
    std::vector<const std::vector<Vec2>*> piece_of(arrangement.sources.size(), nullptr);
    std::vector<bool> reversed(arrangement.sources.size(), false);
    std::vector<bool> has_fit(lines.size(), false);
    for (const detail::FittedChain& chain : fitted) {
        for (std::size_t i = 0; i < chain.edges.size(); ++i) {
            const auto edge = static_cast<std::size_t>(chain.edges[i]);
            piece_of[edge] = &chain.pieces[i];
            reversed[edge] = chain.half_edges[i] % 2 != 0;
            has_fit[static_cast<std::size_t>(arrangement.sources[edge])] = true;
        }
    }
    std::vector<std::vector<std::vector<Vec2>>> parts(lines.size());
    for (std::size_t edge = 0; edge < arrangement.sources.size(); ++edge) {
        const std::int32_t source = arrangement.sources[edge];
        if (source >= 0 && has_fit[static_cast<std::size_t>(source)]) {
            const auto line = static_cast<std::size_t>(source);
            parts[line].push_back(edge_points(arrangement, edge, piece_of[edge], reversed[edge], lines[line]));
        }
    }
    std::vector<std::vector<Vec2>> curves(lines.size());
    for (std::size_t line = 0; line < lines.size(); ++line) {
        if (has_fit[line]) {
            curves[line] = join_parts(std::move(parts[line]), lines[line]);
        }
    }
    return curves;
}

// Fits the curves, with the chain hints that keep the line check: every
// pixel a hinted line owns at 1x stays touched by its stroke. A hint that
// fails is dropped, and its lines are fitted automatically.
struct Curves {
    std::vector<detail::FittedChain> fitted;
    std::vector<std::vector<Vec2>> strokes; // Per line, as stroke_curves.
};

// The pixels each line of a chain hint owns at 1x, per line.
std::vector<std::vector<std::size_t>> hinted_line_pixels(const std::vector<DrawnLine>& lines,
                                                         const std::vector<std::int32_t>& hint_of_line,
                                                         const PlaneView& view) {
    std::vector<std::vector<std::size_t>> pixels(lines.size());
    std::vector<std::int32_t> line_of_primitive;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (hint_of_line[i] < 0) {
            continue;
        }
        if (lines[i].primitive >= line_of_primitive.size()) {
            line_of_primitive.resize(lines[i].primitive + 1, -1);
        }
        line_of_primitive[lines[i].primitive] = static_cast<std::int32_t>(i);
    }
    for (std::size_t i = 0; i < picture_pixels && !line_of_primitive.empty(); ++i) {
        const std::int32_t owner = view.owners[i];
        if (owner < 0 || std::cmp_greater_equal(owner, line_of_primitive.size())) {
            continue;
        }
        if (const std::int32_t line = line_of_primitive[static_cast<std::size_t>(owner)]; line >= 0) {
            pixels[static_cast<std::size_t>(line)].push_back(i);
        }
    }
    return pixels;
}

// Whether every pixel a line owns is touched by its stroke (lines without
// a fitted stroke keep their own).
bool keeps_line_pixels(const std::vector<std::size_t>& pixels, const std::vector<Vec2>& stroke) {
    return stroke.empty() || std::ranges::all_of(pixels, [&](std::size_t i) {
               return stroke_touches(stroke, static_cast<int>(i % picture_width), static_cast<int>(i / picture_width));
           });
}

// Why each chain hint fails after a fitting round (empty where it holds),
// and whether it was fitted at all.
struct HintCheck {
    std::vector<std::string> reasons;
    std::vector<bool> fitted;
};

HintCheck check_hint_chains(const PlaneHints& hints, const Curves& curves, const std::vector<std::int32_t>& failed,
                            const std::vector<std::vector<std::size_t>>& pixels) {
    HintCheck check{
        .reasons = std::vector<std::string>(hints.chains.size()),
        .fitted = std::vector<bool>(hints.chains.size(), false),
    };
    for (const std::int32_t h : failed) {
        check.reasons[static_cast<std::size_t>(h)] = "no such curve within 1 pixel that keeps the faces";
    }
    for (const detail::FittedChain& chain : curves.fitted) {
        if (chain.hint >= 0) {
            check.fitted[static_cast<std::size_t>(chain.hint)] = true;
        }
    }
    for (std::size_t h = 0; h < hints.chains.size(); ++h) {
        if (check.reasons[h].empty() && check.fitted[h] &&
            !std::ranges::all_of(hints.chains[h].lines, [&](std::size_t line) {
                return keeps_line_pixels(pixels[line], curves.strokes[line]);
            })) {
            check.reasons[h] = "the curve misses pixels of its lines";
        }
    }
    return check;
}

Curves fit_with_hints(const detail::Arrangement& arrangement, const std::vector<DrawnLine>& lines,
                      const std::vector<std::optional<TopoSegment>>& segments, const std::vector<int>& owned,
                      const PlaneView& view, const PlaneHints& hints, const std::vector<bool>& curve_lines,
                      SceneStats& stats) {
    std::vector<std::int32_t> hint_of_line(lines.size(), -1);
    std::vector<detail::CurveKind> kinds;
    for (std::size_t h = 0; h < hints.chains.size(); ++h) {
        for (const std::size_t line : hints.chains[h].lines) {
            hint_of_line[line] = static_cast<std::int32_t>(h);
        }
        kinds.push_back(curve_kind(hints.chains[h].hint->primitive));
    }
    const std::vector<std::vector<std::size_t>> pixels = hinted_line_pixels(lines, hint_of_line, view);
    std::vector<bool> enabled(hints.chains.size(), true);
    const auto drop = [&](std::size_t h, std::string reason) {
        enabled[h] = false;
        for (const std::size_t line : hints.chains[h].lines) {
            hint_of_line[line] = -1;
        }
        reject_hint(stats, hints.chains[h].picture, hints.chains[h].hint->id, std::move(reason));
    };
    // A chain hint on lines of a steered curve in the partition: the
    // steered shape replaces its fit.
    for (std::size_t h = 0; h < hints.chains.size(); ++h) {
        if (std::ranges::any_of(hints.chains[h].lines, [&](std::size_t line) { return curve_lines[line]; })) {
            drop(h, "its lines are a steered shape, a curve of the partition");
        }
    }
    // Each round drops at least one hint, or ends.
    while (true) {
        std::vector<std::int32_t> failed;
        Curves curves;
        curves.fitted = fit_curves(arrangement, lines, segments, owned, hint_of_line, kinds, failed, stats);
        curves.strokes = stroke_curves(arrangement, curves.fitted, lines);
        const HintCheck check = check_hint_chains(hints, curves, failed, pixels);
        bool dropped = false;
        for (std::size_t h = 0; h < hints.chains.size(); ++h) {
            if (enabled[h] && !check.reasons[h].empty()) {
                drop(h, check.reasons[h]);
                dropped = true;
            }
        }
        if (dropped) {
            continue;
        }
        for (std::size_t h = 0; h < hints.chains.size(); ++h) {
            if (enabled[h] && check.fitted[h]) {
                ++stats.hint_chains;
                stats.applied_hints.push_back(HintOutcome{
                    .picture = hints.chains[h].picture,
                    .id = hints.chains[h].hint->id,
                    .detail = std::format("{} lines fitted as {}", hints.chains[h].lines.size(),
                                          kind_name(hints.chains[h].hint->primitive)),
                });
            } else if (enabled[h]) {
                drop(h, "its lines form no chain");
            }
        }
        return curves;
    }
}

// The scene model steers the fit (A4 A.4, A.6, A.7; scene_model.hpp): the
// lines of the primitives of decided and certain objects, as one path,
// follow the primitive's shape where the round trip allows it.
// A straight steered shape the partition cannot take by moving vertices:
// one segment of the arrangement in place of its lines' segments.
// How a steered line is in the arrangement: not (its own segment), as one
// segment of a straight shape, or as chords of a curved shape.
enum class Entered : std::uint8_t { No, Segment, Curve };

struct StraightShape {
    std::vector<std::size_t> lines;
    IntPoint a; // Doubled picture coordinates of the shape's ends.
    IntPoint b;
};

// A curved (or off-grid) steered shape the partition cannot take by
// moving vertices: its pieces enter the arrangement as chords.
struct CurvedShape {
    int picture = 0;
    std::string id; // The primitive's, for the report.
    std::vector<std::size_t> lines;
    std::vector<detail::BoundaryLine> pieces; // One per line, as the shared boundary takes them.
};

// Why a curved shape did not enter the arrangement.
struct CurveRefusal {
    int picture = 0;
    std::string id;
    std::string reason;
};

struct SceneModelFits {
    std::vector<bool> steered;                                        // Per line.
    std::vector<StraightShape> straight;                              // Shapes to enter the arrangement.
    std::vector<CurvedShape> curved;                                  // Shapes to enter as chords.
    std::vector<FittedChain> records;                                 // One per steered primitive.
    std::vector<std::pair<std::size_t, detail::StepCut>> square_ends; // Lines' ends at step corners.
    std::vector<std::pair<std::size_t, pic::ScreenPoint>> inner_ends; // Where the lines of a shape meet.
};

std::string_view primitive_name(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::Line:
        return "line";
    case PrimitiveKind::Polyline:
        return "polyline";
    case PrimitiveKind::Ellipse:
        return "ellipse";
    case PrimitiveKind::Arc:
        return "arc";
    case PrimitiveKind::Spline:
        return "spline";
    }
    return "spline";
}

FittedKind fitted_kind(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::Line:
        return FittedKind::Line;
    case PrimitiveKind::Polyline:
        return FittedKind::Polyline;
    case PrimitiveKind::Ellipse:
        return FittedKind::Ellipse;
    case PrimitiveKind::Arc:
        return FittedKind::Arc;
    case PrimitiveKind::Spline:
        return FittedKind::Spline;
    }
    return FittedKind::Spline;
}

// The input of one primitive, with the plane's line of each command;
// nothing if a command draws no line here, or only a dot.
std::optional<std::pair<detail::SteerInput, std::vector<std::size_t>>> steer_input(const HintLines& find,
                                                                                   std::size_t picture,
                                                                                   const std::vector<DrawnLine>& lines,
                                                                                   const PrimitiveHint& primitive) {
    detail::SteerInput input{
        .kind = primitive.kind,
        .lines = {},
        .points = {},
        .corner = {},
        .tolerance = primitive.tolerance != 0 ? primitive.tolerance : detail::tolerance_unit,
        .dots = {},
    };
    std::vector<std::size_t> line_of;
    // A dot of an open ellipse or an arc keeps its stroke and bridges the
    // gap it fills (scene_model.hpp).
    const bool open_curve = primitive.kind == PrimitiveKind::Arc ||
                            (primitive.kind == PrimitiveKind::Ellipse && primitive.points.size() >= 3 &&
                             primitive.points.front() != primitive.points.back());
    for (const LineRef ref : primitive.commands) {
        const auto line = find.line(picture, ref);
        if (line && lines[*line].is_dot() && open_curve) {
            input.dots.push_back(lines[*line].from);
            continue;
        }
        if (!line || lines[*line].is_dot()) {
            return std::nullopt;
        }
        line_of.push_back(*line);
        input.lines.push_back(detail::SteerLine{.from = lines[*line].from, .to = lines[*line].to});
    }
    for (std::size_t k = 0; k < primitive.points.size(); ++k) {
        input.points.push_back(Vec2{.x = primitive.points[k].x + 0.5, .y = primitive.points[k].y + 0.5});
        const auto corner = std::ranges::find(primitive.corners, k, &CornerHint::point);
        input.corner.push_back(corner == primitive.corners.end() ? std::nullopt : std::optional{corner->kind});
    }
    return std::pair{std::move(input), std::move(line_of)};
}

// A primitive whose shape passed its checks and changes its strokes,
// before it is applied.
struct PendingShape {
    const PrimitiveHint* primitive = nullptr;
    std::vector<std::size_t> line_of;
    detail::Steered result;
    bool decided = false; // The customer decided the object.
};

// One primitive's shape, if it passes the round trip and the line check
// and changes its lines' strokes; otherwise the report says why, if it
// was tried.
// With `solved` (the relation solver's anchors), the shape runs through
// them, and a failure is written to `failure` instead of the report.
std::optional<PendingShape> steer_primitive(const detail::SteerInput& input, const std::vector<std::size_t>& line_of,
                                            int picture, const PrimitiveHint& primitive, bool decided,
                                            const std::vector<DrawnLine>& lines,
                                            const std::vector<std::vector<std::size_t>>& pixels, const Curves& curves,
                                            const SceneModelFits& model, SceneStats& stats,
                                            const std::vector<Vec2>* solved = nullptr, std::string* failure = nullptr) {
    const auto fail = [&](std::string why) {
        if (failure != nullptr) {
            *failure = std::format("{}: {}", primitive.id, why);
        } else {
            reject_hint(stats, picture, primitive.id, std::format("{}: the 3b reconstruction stays", why));
        }
    };
    // A line in two primitives follows the first.
    if (std::ranges::any_of(line_of, [&](std::size_t line) { return model.steered[line]; })) {
        if (failure != nullptr) {
            *failure = std::format("{}: a line is steered already", primitive.id);
        }
        return std::nullopt;
    }
    detail::Steered result = detail::steer(input, solved);
    if (!result.applicable) {
        if (failure != nullptr) {
            *failure = std::format("{}: not steered", primitive.id);
        }
        return std::nullopt;
    }
    if (result.pieces.empty()) {
        fail(result.reason);
        return std::nullopt;
    }
    // As for a chain hint (spec section 6.4): every pixel the lines own at
    // 1x is still touched by its stroke; for a shape the customer decided,
    // or passed within the A.6 tolerance (the approved deviation).
    for (std::size_t i = 0; i < line_of.size(); ++i) {
        const std::vector<Vec2>& piece = result.pieces[i];
        const bool kept = std::ranges::all_of(pixels[line_of[i]], [&](std::size_t pixel) {
            const int x = static_cast<int>(pixel % picture_width);
            const int y = static_cast<int>(pixel / picture_width);
            return stroke_touches(piece, x, y) ||
                   (decided && within_round_trip_tolerance(piece, x, y, detail::raster_reach(input.tolerance)));
        });
        if (!kept) {
            fail("the shape misses pixels its lines own");
            return std::nullopt;
        }
    }
    bool changed = false;
    for (std::size_t i = 0; i < line_of.size(); ++i) {
        const DrawnLine& line = lines[line_of[i]];
        const std::vector<Vec2> current = curves.strokes[line_of[i]].empty()
                                              ? std::vector<Vec2>{centre(line.from), centre(line.to)}
                                              : curves.strokes[line_of[i]];
        changed = changed || current != result.pieces[i];
    }
    if (!changed) {
        return std::nullopt;
    }
    return PendingShape{.primitive = &primitive, .line_of = line_of, .result = std::move(result), .decided = decided};
}

// Spec section 6.6: the shape as the boundary of the faces on both sides.
std::vector<detail::BoundaryLine> boundary_lines(const PendingShape& shape, const std::vector<DrawnLine>& lines) {
    std::vector<detail::BoundaryLine> shared;
    for (std::size_t i = 0; i < shape.line_of.size(); ++i) {
        const DrawnLine& line = lines[shape.line_of[i]];
        shared.push_back(detail::BoundaryLine{
            .source = static_cast<std::int32_t>(shape.line_of[i]),
            .from = centre(line.from),
            .to = centre(line.to),
            .piece = shape.result.pieces[i],
        });
    }
    return shared;
}

// Makes the shape the lines' strokes and records it; `why` is why the
// partition could not take it (empty: the faces share it).
void apply_shape(PendingShape& shape, int picture, const std::string& why, std::string_view entered,
                 const std::vector<DrawnLine>& lines, Curves& curves, SceneModelFits& model, SceneStats& stats) {
    const PrimitiveHint& primitive = *shape.primitive;
    // The ends where the shape's lines meet (on or next to another line's
    // end) are inside the shape: the stroke integrity pass leaves them.
    for (const std::size_t line : shape.line_of) {
        for (const pic::ScreenPoint end : {lines[line].from, lines[line].to}) {
            const bool inner = std::ranges::any_of(shape.line_of, [&](std::size_t other) {
                return other != line &&
                       std::ranges::any_of(std::array{lines[other].from, lines[other].to}, [&](pic::ScreenPoint e) {
                           return std::abs(e.x - end.x) <= 1 && std::abs(e.y - end.y) <= 1;
                       });
            });
            if (inner) {
                model.inner_ends.emplace_back(line, end);
            }
        }
    }
    FittedChain record{.kind = fitted_kind(primitive.kind), .hint = primitive.id, .lines = {}};
    for (std::size_t i = 0; i < shape.line_of.size(); ++i) {
        curves.strokes[shape.line_of[i]] = std::move(shape.result.pieces[i]);
        model.steered[shape.line_of[i]] = true;
        record.lines.push_back(static_cast<std::int32_t>(lines[shape.line_of[i]].primitive));
        if (shape.decided) {
            stats.decided_lines.push_back(static_cast<std::int32_t>(lines[shape.line_of[i]].primitive));
        }
        if (shape.decided && primitive.tolerance > detail::tolerance_unit) {
            stats.widened_lines.push_back(static_cast<std::int32_t>(lines[shape.line_of[i]].primitive));
        }
    }
    std::ranges::stable_sort(record.lines);
    const auto [first, last] = std::ranges::unique(record.lines);
    record.lines.erase(first, last);
    model.records.push_back(std::move(record));
    ++stats.scene_model_fits;
    stats.applied_hints.push_back(HintOutcome{
        .picture = picture,
        .id = primitive.id,
        .detail = (why.empty() ? std::format("drawn as its {} (round trip {}), shared by the faces{}",
                                             primitive_name(primitive.kind), shape.result.deviation, entered)
                               : std::format("drawn as its {} (round trip {}); the faces keep the 3b fit: the shared "
                                             "boundary fails ({})",
                                             primitive_name(primitive.kind), shape.result.deviation, why)) +
                  (shape.result.widened ? std::format(" (within the customer's tolerance of {}.{:03} px)",
                                                      primitive.tolerance / detail::tolerance_unit,
                                                      primitive.tolerance % detail::tolerance_unit)
                                        : std::string{}),
    });
}

// A `line` shape as one segment of the arrangement, if its ends lie on the
// doubled grid (pixel centres); nothing for other kinds.
std::optional<StraightShape> straight_shape(const PendingShape& shape) {
    if (shape.primitive->kind != PrimitiveKind::Line || shape.result.pieces.empty()) {
        return std::nullopt;
    }
    const auto on_grid = [](Vec2 p) -> std::optional<IntPoint> {
        const double x = 2 * p.x;
        const double y = 2 * p.y;
        if (x != std::floor(x) || y != std::floor(y)) {
            return std::nullopt;
        }
        return IntPoint{.x = static_cast<std::int64_t>(x), .y = static_cast<std::int64_t>(y)};
    };
    // The path's ends: of the first and the last piece, the end away from
    // the piece next to it (a line may run against the path).
    const auto& pieces = shape.result.pieces;
    const auto outer_end = [](const std::vector<Vec2>& piece, const std::vector<Vec2>& next) {
        const auto gap = [&](Vec2 p) {
            const auto d = [&](Vec2 q) { return ((p.x - q.x) * (p.x - q.x)) + ((p.y - q.y) * (p.y - q.y)); };
            return std::min(d(next.front()), d(next.back()));
        };
        return gap(piece.front()) >= gap(piece.back()) ? piece.front() : piece.back();
    };
    const Vec2 first = pieces.size() == 1 ? pieces.front().front() : outer_end(pieces.front(), pieces[1]);
    const Vec2 last = pieces.size() == 1 ? pieces.front().back() : outer_end(pieces.back(), pieces[pieces.size() - 2]);
    const auto a = on_grid(first);
    const auto b = on_grid(last);
    if (!a || !b || *a == *b) {
        return std::nullopt;
    }
    return StraightShape{.lines = shape.line_of, .a = *a, .b = *b};
}

// Makes the shapes of one object the boundary of their faces, jointly if
// the partition takes them so, else each on its own, and applies them.
// How many of them keep the 3b faces (the partition could not take them).
int share_shapes(std::vector<PendingShape>& shapes, int picture, const std::vector<DrawnLine>& lines,
                 const std::vector<Entered>& in_arrangement, Curves& curves, detail::SharedBoundary& boundary,
                 SceneModelFits& model, SceneStats& stats) {
    if (shapes.empty()) {
        return 0;
    }
    // Straight shapes already in the arrangement are its boundary.
    const auto entered = [&](const PendingShape& shape) {
        return std::ranges::all_of(shape.line_of,
                                   [&](std::size_t line) { return in_arrangement[line] != Entered::No; });
    };
    std::vector<std::string> why(shapes.size());
    bool joint = false;
    if (std::ranges::count_if(shapes, [&](const PendingShape& shape) { return !entered(shape); }) > 1) {
        std::vector<detail::BoundaryLine> all;
        for (const PendingShape& shape : shapes) {
            if (!entered(shape)) {
                std::ranges::move(boundary_lines(shape, lines), std::back_inserter(all));
            }
        }
        joint = boundary.apply(all).empty();
    }
    if (!joint) {
        for (std::size_t i = 0; i < shapes.size(); ++i) {
            if (!entered(shapes[i])) {
                why[i] = boundary.apply(boundary_lines(shapes[i], lines));
            }
        }
    }
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        if (!why[i].empty()) {
            if (auto straight = straight_shape(shapes[i])) {
                model.straight.push_back(std::move(*straight));
            } else {
                model.curved.push_back(CurvedShape{
                    .picture = picture,
                    .id = shapes[i].primitive->id,
                    .lines = shapes[i].line_of,
                    .pieces = boundary_lines(shapes[i], lines),
                });
            }
        }
        std::string_view note;
        if (entered(shapes[i])) {
            note = in_arrangement[shapes[i].line_of.front()] == Entered::Curve ? " (a curve of the partition)"
                                                                               : " (one segment of the partition)";
        }
        apply_shape(shapes[i], picture, why[i], note, lines, curves, model, stats);
    }
    return static_cast<int>(std::ranges::count_if(why, [](const std::string& w) { return !w.empty(); }));
}

// A primitive of the followed hypothesis whose commands draw lines here.
struct ObjectPrimitive {
    const PrimitiveHint* hint = nullptr;
    detail::SteerInput input;
    std::vector<std::size_t> line_of;
};

std::string_view relation_name(const GeometricRelation& relation) {
    switch (relation.kind) {
    case GeometricKind::Parallel:
        return "parallel";
    case GeometricKind::Perpendicular:
        return "perpendicular";
    case GeometricKind::EqualSpacing:
        return "equal spacing";
    case GeometricKind::Symmetry:
        return relation.axis == Axis::Vertical ? "symmetry about a vertical axis" : "symmetry about a horizontal axis";
    case GeometricKind::Tangent:
        return "tangent";
    case GeometricKind::Aligned:
        return relation.axis == Axis::Vertical ? "aligned vertically" : "aligned horizontally";
    }
    return "relation";
}

// The index of the hint point an anchor stands for: an end anchor the
// nearer end point, an inner one (a polyline corner) the nearest inner
// point (Chebyshev, in pixel centres).
std::size_t hint_point_of(const PrimitiveHint& hint, const std::vector<Vec2>& anchors, std::size_t k) {
    const auto distance = [&](std::size_t i) {
        const Vec2 c{.x = hint.points[i].x + 0.5, .y = hint.points[i].y + 0.5};
        return std::max(std::abs(c.x - anchors[k].x), std::abs(c.y - anchors[k].y));
    };
    const std::size_t last = hint.points.size() - 1;
    if (k == 0 || k + 1 == anchors.size() || hint.points.size() <= 2) {
        const bool front = k == 0 ? distance(0) <= distance(last) : distance(0) < distance(last);
        return front ? 0 : last;
    }
    std::size_t best = 1;
    for (std::size_t i = 2; i < last; ++i) {
        if (distance(i) < distance(best)) {
            best = i;
        }
    }
    return best;
}

// A4 A.3: the relations of the followed hypothesis, solved jointly over
// the anchors of its line and polyline primitives (relations.hpp). Per
// primitive, its solved anchors, or nothing (not in a solved relation, or
// nothing solved). An object whose main hypothesis the customer chose over
// alternatives keeps its relations unsolved: that is the "as drawn"
// reading (picture 12's tall window: "keep as drawn"); a chosen
// alternative has its relations solved (keeps_relations_as_drawn).
// NOLINTNEXTLINE(readability-function-cognitive-complexity): resolving the items and ties shares the problem.
std::vector<std::optional<std::vector<Vec2>>> solve_object(const ObjectHint& object, const Hypothesis& hypothesis,
                                                           const std::vector<ObjectPrimitive>& found, int picture,
                                                           SceneStats& stats) {
    std::vector<std::optional<std::vector<Vec2>>> solved(found.size());
    if (hypothesis.relations.empty()) {
        return solved;
    }
    if (keeps_relations_as_drawn(object)) {
        reject_hint(stats, picture, object.id,
                    "relations not solved: the customer chose the main hypothesis over its alternatives, as drawn");
        return solved;
    }
    detail::RelationProblem problem;
    std::vector<std::size_t> found_of; // Per problem primitive.
    std::vector<std::optional<std::size_t>> problem_of(found.size());
    const auto primitive_index = [&](const std::string& id) -> std::optional<std::size_t> {
        const auto at = std::ranges::find_if(found, [&](const ObjectPrimitive& f) { return f.hint->id == id; });
        if (at == found.end()) {
            return std::nullopt;
        }
        const auto i = static_cast<std::size_t>(at - found.begin());
        if (!problem_of[i]) {
            const auto points = detail::anchors(at->input);
            if (!points) {
                return std::nullopt;
            }
            problem_of[i] = problem.primitives.size();
            found_of.push_back(i);
            problem.primitives.push_back(detail::SolvePrimitive{.kind = at->input.kind, .anchors = *points});
        }
        return problem_of[i];
    };
    std::vector<std::string_view> names;
    for (std::size_t r = 0; r < hypothesis.relations.size(); ++r) {
        const GeometricRelation& relation = hypothesis.relations[r];
        if (relation.kind == GeometricKind::Tangent) {
            reject_hint(stats, picture, object.id,
                        std::format("relation {} (tangent) not solved: tangent is not solved yet", r));
            continue;
        }
        detail::SolveRelation solve{.kind = relation.kind, .axis = relation.axis, .items = {}};
        bool usable = true;
        for (const std::string& id : relation.items) {
            std::vector<std::size_t> item;
            const auto part = std::ranges::find(hypothesis.parts, id, &PartHint::id);
            if (part != hypothesis.parts.end()) {
                for (const PrimitiveHint& primitive : part->primitives) {
                    const auto index = primitive_index(primitive.id);
                    usable = usable && index.has_value();
                    if (index) {
                        item.push_back(*index);
                    }
                }
            } else if (const auto index = primitive_index(id)) {
                item.push_back(*index);
            } else {
                usable = false;
            }
            usable = usable && !item.empty();
            solve.items.push_back(std::move(item));
        }
        if (!usable) {
            reject_hint(stats, picture, object.id,
                        std::format("relation {} ({}) not solved: an item is not a steered line or polyline", r,
                                    relation_name(relation)));
            continue;
        }
        problem.relations.push_back(std::move(solve));
        names.push_back(relation_name(relation));
    }
    if (problem.relations.empty()) {
        return solved;
    }
    // Points the hint gives equal in two primitives are one point (a shared
    // corner).
    for (std::size_t a = 0; a < problem.primitives.size(); ++a) {
        for (std::size_t b = a + 1; b < problem.primitives.size(); ++b) {
            const PrimitiveHint& ha = *found[found_of[a]].hint;
            const PrimitiveHint& hb = *found[found_of[b]].hint;
            for (std::size_t i = 0; i < problem.primitives[a].anchors.size(); ++i) {
                for (std::size_t j = 0; j < problem.primitives[b].anchors.size(); ++j) {
                    if (ha.points[hint_point_of(ha, problem.primitives[a].anchors, i)] ==
                        hb.points[hint_point_of(hb, problem.primitives[b].anchors, j)]) {
                        problem.ties.emplace_back(detail::AnchorRef{.primitive = a, .point = i},
                                                  detail::AnchorRef{.primitive = b, .point = j});
                    }
                }
            }
        }
    }
    const detail::RelationSolution solution = detail::solve_relations(problem);
    std::string list;
    for (const std::string_view name : names) {
        list += list.empty() ? std::string(name) : std::format(", {}", name);
    }
    if (!solution.solved) {
        reject_hint(
            stats, picture, object.id,
            std::format("relations ({}) not solved: {}: the unsolved shapes are steered", list, solution.reason));
        return solved;
    }
    if (solution.largest_move == 0) {
        return solved; // They hold as drawn.
    }
    for (std::size_t q = 0; q < problem.primitives.size(); ++q) {
        solved[found_of[q]] = solution.anchors[q];
    }
    stats.applied_hints.push_back(HintOutcome{
        .picture = picture,
        .id = object.id,
        .detail = std::format("relations solved jointly ({}), largest move {:.2f} px", list, solution.largest_move),
    });
    return solved;
}

// The primitives of the hypothesis an object follows (A.7), if it is not
// organic (A.4). An open object, or one decided "keep", steers nothing.
// Its relations are solved first (A.3, solve_object); if a solved shape
// fails its checks, every primitive is steered unsolved, and the report
// says why.
// The shapes of one object are placed jointly, so that the vertices they
// share move together; where the partition cannot take them all, each is
// tried on its own, in order. Where the partition cannot take a shape, its
// stroke is still steered (the state before the shared boundary) and the
// faces keep the 3b fit; the report says so.
// NOLINTNEXTLINE(readability-function-cognitive-complexity): the solved and unsolved placements share their state.
void steer_object(const HintLines& find, std::size_t p, const PictureHints& picture, const ObjectHint& object,
                  const std::vector<DrawnLine>& lines, const std::vector<std::vector<std::size_t>>& pixels,
                  const std::vector<Entered>& in_arrangement, Curves& curves, detail::SharedBoundary& boundary,
                  SceneModelFits& model, SceneStats& stats) {
    const Hypothesis* hypothesis = followed_hypothesis(object);
    if (hypothesis == nullptr || is_organic(hypothesis->type)) {
        return;
    }
    std::vector<ObjectPrimitive> found;
    for (const PartHint& part : hypothesis->parts) {
        for (const PrimitiveHint& primitive : part.primitives) {
            auto input = steer_input(find, p, lines, primitive);
            if (!input) {
                continue;
            }
            for (const detail::StepCut& cut : detail::step_cuts(input->first)) {
                model.square_ends.emplace_back(input->second[cut.line], cut);
            }
            found.push_back(ObjectPrimitive{
                .hint = &primitive,
                .input = std::move(input->first),
                .line_of = std::move(input->second),
            });
        }
    }
    const bool decided = object.decision.has_value();
    std::vector<std::optional<std::vector<Vec2>>> solved =
        solve_object(object, *hypothesis, found, picture.picture, stats);
    // The solved shapes must pass the same checks; otherwise none is used.
    for (std::size_t i = 0; i < found.size(); ++i) {
        const std::optional<std::vector<Vec2>>& entry = solved[i];
        const std::vector<Vec2>* anchors = entry.has_value() ? &*entry : nullptr;
        if (anchors == nullptr) {
            continue;
        }
        std::string failure;
        (void)steer_primitive(found[i].input, found[i].line_of, picture.picture, *found[i].hint, decided, lines, pixels,
                              curves, model, stats, anchors, &failure);
        if (!failure.empty()) {
            reject_hint(stats, picture.picture, object.id,
                        std::format("the solved relations fail ({}): the unsolved shapes are steered", failure));
            std::ranges::fill(solved, std::nullopt);
            break;
        }
    }
    // Places the shapes (with the solved anchors where given); how many keep
    // the 3b faces.
    const auto place = [&](const std::vector<std::optional<std::vector<Vec2>>>& use, Curves& to_curves,
                           detail::SharedBoundary& to_boundary, SceneModelFits& to_model, SceneStats& to_stats) {
        std::vector<PendingShape> shapes;
        std::vector<bool> taken(lines.size(), false); // Lines of an earlier shape of this object.
        for (std::size_t i = 0; i < found.size(); ++i) {
            const ObjectPrimitive& primitive = found[i];
            if (std::ranges::any_of(primitive.line_of, [&](std::size_t line) { return taken[line]; })) {
                continue; // A line in two primitives follows the first.
            }
            if (auto shape =
                    steer_primitive(primitive.input, primitive.line_of, picture.picture, *primitive.hint, decided,
                                    lines, pixels, to_curves, to_model, to_stats, use[i] ? &*use[i] : nullptr)) {
                for (const std::size_t line : primitive.line_of) {
                    taken[line] = true;
                }
                shapes.push_back(std::move(*shape));
            }
        }
        // A corner the solver made one point of two shapes (tied points) is
        // inside the object: the stroke integrity pass leaves those ends, as it
        // leaves the inner ends of one shape.
        if (std::ranges::any_of(use, [](const auto& anchors) { return anchors.has_value(); })) {
            for (std::size_t s = 0; s < shapes.size(); ++s) {
                for (std::size_t i = 0; i < shapes[s].line_of.size(); ++i) {
                    const std::vector<Vec2>& piece = shapes[s].result.pieces[i];
                    const DrawnLine& line = lines[shapes[s].line_of[i]];
                    for (const auto& [end, point] :
                         {std::pair{line.from, piece.front()}, std::pair{line.to, piece.back()}}) {
                        const bool shared = std::ranges::any_of(shapes, [&](const PendingShape& other) {
                            return &other != &shapes[s] &&
                                   std::ranges::any_of(other.result.pieces, [&](const std::vector<Vec2>& q) {
                                       const auto same = [&](Vec2 r) {
                                           return std::abs(r.x - point.x) < 1e-6 && std::abs(r.y - point.y) < 1e-6;
                                       };
                                       return same(q.front()) || same(q.back());
                                   });
                        });
                        if (shared) {
                            to_model.inner_ends.emplace_back(shapes[s].line_of[i], end);
                        }
                    }
                }
            }
        }
        return share_shapes(shapes, picture.picture, lines, in_arrangement, to_curves, to_boundary, to_model, to_stats);
    };
    const bool any_solved = std::ranges::any_of(solved, [](const auto& anchors) { return anchors.has_value(); });
    if (!any_solved || decided) {
        // The customer's decision asks for its relations: a solved shape the
        // partition cannot take keeps its stroke, as any decided shape.
        (void)place(solved, curves, boundary, model, stats);
        return;
    }
    // An analyser's hypothesis (certain, not decided): solving must not cost
    // the shared boundary; where it does, the unsolved shapes are steered.
    Curves solved_curves = curves;
    detail::SharedBoundary solved_boundary = boundary;
    SceneModelFits solved_model = model;
    SceneStats solved_stats = stats;
    const int solved_keep = place(solved, solved_curves, solved_boundary, solved_model, solved_stats);
    const std::vector<std::optional<std::vector<Vec2>>> unsolved(found.size());
    Curves unsolved_curves = curves;
    detail::SharedBoundary unsolved_boundary = boundary;
    SceneModelFits unsolved_model = model;
    SceneStats unsolved_stats = stats;
    const int unsolved_keep = place(unsolved, unsolved_curves, unsolved_boundary, unsolved_model, unsolved_stats);
    if (solved_keep > unsolved_keep) {
        std::erase_if(unsolved_stats.applied_hints, [&](const HintOutcome& outcome) {
            return outcome.id == object.id && outcome.detail.starts_with("relations solved");
        });
        reject_hint(unsolved_stats, picture.picture, object.id,
                    std::format("the solved relations cost the shared boundary ({} shapes keep the 3b faces, "
                                "unsolved {}): the unsolved shapes are steered",
                                solved_keep, unsolved_keep));
        curves = std::move(unsolved_curves);
        boundary = std::move(unsolved_boundary);
        model = std::move(unsolved_model);
        stats = std::move(unsolved_stats);
    } else {
        curves = std::move(solved_curves);
        boundary = std::move(solved_boundary);
        model = std::move(solved_model);
        stats = std::move(solved_stats);
    }
}

SceneModelFits steer_by_scene_model(const pic::DrawTrace& trace, const std::vector<DrawnLine>& lines,
                                    const PlaneView& view, std::span<const PictureHints* const> hints,
                                    const std::vector<Entered>& in_arrangement, Curves& curves,
                                    detail::SharedBoundary& boundary, SceneStats& stats) {
    SceneModelFits model{
        .steered = std::vector<bool>(lines.size(), false),
        .straight = {},
        .curved = {},
        .records = {},
        .square_ends = {},
        .inner_ends = {},
    };
    const HintLines find(trace, lines);
    // The pixels each line owns at 1x (every line counts).
    const std::vector<std::vector<std::size_t>> pixels =
        hinted_line_pixels(lines, std::vector<std::int32_t>(lines.size(), 0), view);
    for (std::size_t p = 0; p < hints.size() && p < trace.pictures; ++p) {
        if (hints[p] == nullptr) {
            continue;
        }
        for (const ObjectHint& object : hints[p]->objects) {
            steer_object(find, p, *hints[p], object, lines, pixels, in_arrangement, curves, boundary, model, stats);
        }
    }
    return model;
}

// Puts each straight shape into the topology as one segment in place of
// its lines' segments; the connectors at the lines' inner ends go, as
// those ends are no longer drawn. A shape is kept only if the partition
// gets no more mixed faces or 1x-block pixels (the rule of gap hints).
// Whether any shape was put in.
bool enter_straight_shapes(std::vector<TopoSegment>& topology, const std::vector<StraightShape>& shapes,
                           const std::vector<DrawnLine>& lines, Partitioner& partition,
                           std::vector<Entered>& in_arrangement) {
    bool entered = false;
    Partition current = partition(topology);
    for (const StraightShape& shape : shapes) {
        std::vector<bool> own(lines.size(), false);
        std::vector<IntPoint> inner_ends;
        for (const std::size_t line : shape.lines) {
            own[line] = true;
            for (const pic::ScreenPoint end : {lines[line].from, lines[line].to}) {
                if (const IntPoint p = doubled(end); p != shape.a && p != shape.b) {
                    inner_ends.push_back(p);
                }
            }
        }
        const auto at_inner_end = [&](const RationalPoint& p) {
            return std::ranges::any_of(inner_ends, [&](IntPoint e) { return RationalPoint(e) == p; });
        };
        std::vector<TopoSegment> candidate;
        for (const TopoSegment& segment : topology) {
            const bool own_line = segment.source >= 0 && std::cmp_less(segment.source, lines.size()) &&
                                  own[static_cast<std::size_t>(segment.source)];
            const bool inner_connector =
                segment.source == detail::no_source && (at_inner_end(segment.from) || at_inner_end(segment.to));
            if (!own_line && !inner_connector) {
                candidate.push_back(segment);
            }
        }
        candidate.push_back(detail::make_segment(shape.a, shape.b));
        Partition with = partition(candidate);
        if (no_worse(with, current)) {
            topology = std::move(candidate);
            current = std::move(with);
            for (const std::size_t line : shape.lines) {
                in_arrangement[line] = Entered::Segment;
            }
            entered = true;
        }
    }
    return entered;
}

// The scale at which curved shapes enter the arrangement: 1/16 original
// pixel (docs/hd-pipeline.md, "Curved shapes in the arrangement").
constexpr std::int64_t curve_scale = detail::max_scale;

// The point of the fine grid (units of 1 / (2 * curve_scale) pixels)
// nearest to a point in picture coordinates.
IntPoint fine_point(Vec2 p) {
    constexpr double units = 2.0 * curve_scale;
    return IntPoint{.x = core::round_half_up(p.x * units), .y = core::round_half_up(p.y * units)};
}

// Whether the point lies on the segment (exactly).
bool on_segment(const TopoSegment& segment, const RationalPoint& p) {
    if (core::orientation(segment.a, segment.b, p) != 0) {
        return false;
    }
    const RationalPoint low = std::min(segment.from, segment.to);
    const RationalPoint high = std::max(segment.from, segment.to);
    return !(p < low) && !(high < p);
}

// The topology with one curved shape entered, in fine units, or why not.
// `topology` is in fine units already.
// NOLINTNEXTLINE(readability-function-cognitive-complexity): one pass over the topology, case by case.
std::variant<std::vector<TopoSegment>, std::string> with_curve(const std::vector<TopoSegment>& topology,
                                                               const CurvedShape& shape,
                                                               const std::vector<DrawnLine>& lines,
                                                               const std::vector<std::optional<TopoSegment>>& own) {
    std::vector<bool> mine(lines.size(), false);
    for (const std::size_t line : shape.lines) {
        mine[line] = true;
    }
    // The shape's old segments (as the topology had them), fine units.
    std::vector<TopoSegment> old;
    for (const std::size_t line : shape.lines) {
        if (own[line]) {
            old.push_back(scaled(*own[line], curve_scale));
        }
    }
    // The path's ends: line ends not on or next to another line's end of
    // the shape. They stay exactly where they are.
    std::vector<pic::ScreenPoint> outer;
    std::vector<pic::ScreenPoint> inner_ends;
    for (const std::size_t line : shape.lines) {
        for (const pic::ScreenPoint end : {lines[line].from, lines[line].to}) {
            const bool inner = std::ranges::any_of(shape.lines, [&](std::size_t other) {
                return other != line &&
                       std::ranges::any_of(std::array{lines[other].from, lines[other].to}, [&](pic::ScreenPoint e) {
                           return std::abs(e.x - end.x) <= 1 && std::abs(e.y - end.y) <= 1;
                       });
            });
            (inner ? inner_ends : outer).push_back(end);
        }
    }
    const auto fine_end = [&](pic::ScreenPoint p) {
        return scaled(detail::make_segment(doubled(p), {}), curve_scale).a;
    };
    const auto is_outer = [&](const RationalPoint& p) {
        return std::ranges::any_of(outer, [&](pic::ScreenPoint e) { return RationalPoint(fine_end(e)) == p; });
    };
    // The chords: each line's piece, its points on the fine grid; an end at
    // a path end is the pixel centre itself.
    std::vector<TopoSegment> chords;
    for (const detail::BoundaryLine& piece : shape.pieces) {
        const DrawnLine& line = lines[static_cast<std::size_t>(piece.source)];
        std::vector<IntPoint> points;
        points.reserve(piece.piece.size());
        for (const Vec2 p : piece.piece) {
            points.push_back(fine_point(p));
        }
        if (points.empty()) {
            return std::string("a piece is empty");
        }
        for (IntPoint* end : {&points.front(), &points.back()}) {
            for (const pic::ScreenPoint e : {line.from, line.to}) {
                const IntPoint centre_point = fine_end(e);
                const bool outer_end = std::ranges::find(outer, e) != outer.end();
                if (outer_end &&
                    std::max(std::abs(end->x - centre_point.x), std::abs(end->y - centre_point.y)) <= curve_scale) {
                    *end = centre_point;
                }
            }
        }
        for (std::size_t k = 0; k + 1 < points.size(); ++k) {
            if (points[k] != points[k + 1]) {
                chords.push_back(detail::make_segment(points[k], points[k + 1]));
            }
        }
    }
    if (chords.empty()) {
        return std::string("no chords");
    }
    const auto on_old = [&](const RationalPoint& p) {
        return std::ranges::any_of(old, [&](const TopoSegment& segment) { return on_segment(segment, p); });
    };
    // The nearest crossing of the segment's line with the chords, to p,
    // within 2 original pixels.
    const auto to_curve = [&](const TopoSegment& segment, const RationalPoint& p) -> std::optional<RationalPoint> {
        std::optional<RationalPoint> best;
        double best_distance = 0;
        const Vec2 from = detail::to_picture(p);
        for (const TopoSegment& chord : chords) {
            const auto crossing = detail::line_meets_segment(segment.a, segment.b, chord, curve_scale);
            if (!crossing) {
                continue;
            }
            const Vec2 at = detail::to_picture(*crossing);
            // Fine units: picture coordinates times curve_scale.
            const double distance = ((at.x - from.x) * (at.x - from.x)) + ((at.y - from.y) * (at.y - from.y));
            if (distance <= 4.0 * curve_scale * curve_scale && (!best || distance < best_distance)) {
                best = crossing;
                best_distance = distance;
            }
        }
        return best;
    };
    // A connector from a grid point to the chords' nearest point, on the
    // grid, within 2 original pixels; nothing if that is the point itself.
    const auto to_nearest = [&](IntPoint p) -> std::optional<TopoSegment> {
        const Vec2 from{.x = static_cast<double>(p.x), .y = static_cast<double>(p.y)};
        std::optional<Vec2> best;
        double best_distance = 0;
        for (const TopoSegment& chord : chords) {
            const Vec2 a{.x = static_cast<double>(chord.a.x), .y = static_cast<double>(chord.a.y)};
            const Vec2 d{.x = static_cast<double>(chord.b.x) - a.x, .y = static_cast<double>(chord.b.y) - a.y};
            const double t =
                std::clamp((((from.x - a.x) * d.x) + ((from.y - a.y) * d.y)) / ((d.x * d.x) + (d.y * d.y)), 0.0, 1.0);
            const Vec2 q{.x = a.x + (t * d.x), .y = a.y + (t * d.y)};
            const double distance = ((q.x - from.x) * (q.x - from.x)) + ((q.y - from.y) * (q.y - from.y));
            if (!best || distance < best_distance) {
                best = q;
                best_distance = distance;
            }
        }
        // Fine units: 2 original pixels are 4 * curve_scale.
        constexpr double limit = 4.0 * curve_scale;
        if (!best || best_distance > limit * limit) {
            return std::nullopt;
        }
        const IntPoint q{.x = core::round_half_up(best->x), .y = core::round_half_up(best->y)};
        if (q == p) {
            return std::nullopt;
        }
        return detail::make_segment(p, q);
    };
    std::vector<TopoSegment> result;
    for (const TopoSegment& segment : topology) {
        const bool own_line = segment.source >= 0 && std::cmp_less(segment.source, lines.size()) &&
                              mine[static_cast<std::size_t>(segment.source)];
        if (own_line) {
            // The gap closer's extension at a path end stays, from the
            // pixel centre on along the line.
            const DrawnLine& line = lines[static_cast<std::size_t>(segment.source)];
            for (const auto& [e, at] : {std::pair{line.from, segment.from}, std::pair{line.to, segment.to}}) {
                const RationalPoint end(fine_end(e));
                if (std::ranges::find(outer, e) != outer.end() && at != end) {
                    TopoSegment extension = segment;
                    extension.from = end;
                    extension.to = at;
                    extension.source = detail::no_source;
                    result.push_back(extension);
                }
            }
            continue;
        }
        // The connectors at the lines' inner ends go: those ends are no
        // longer drawn (as for straight shapes).
        const auto at_inner_end = [&](const RationalPoint& p) {
            return std::ranges::any_of(inner_ends, [&](pic::ScreenPoint e) { return RationalPoint(fine_end(e)) == p; });
        };
        // Those are the shape's own: between its lines, or extending one of
        // them. A connector from another line's end to an inner end is moved
        // onto the curve below, like a line ending on the shape.
        const auto extends_own = [&](const TopoSegment& connector) {
            return std::ranges::any_of(old, [&](const TopoSegment& own_segment) {
                return core::orientation(own_segment.a, own_segment.b, connector.from) == 0 &&
                       core::orientation(own_segment.a, own_segment.b, connector.to) == 0;
            });
        };
        if (segment.source == detail::no_source && (at_inner_end(segment.from) || at_inner_end(segment.to)) &&
            ((on_old(segment.from) && on_old(segment.to)) || extends_own(segment))) {
            continue;
        }
        const bool from_on = on_old(segment.from) && !is_outer(segment.from);
        const bool to_on = on_old(segment.to) && !is_outer(segment.to);
        if (!from_on && !to_on) {
            result.push_back(segment);
            continue;
        }
        if (from_on && to_on) {
            continue; // A connector between the shape's own lines.
        }
        TopoSegment moved = segment;
        RationalPoint& end = from_on ? moved.from : moved.to;
        const auto crossing = to_curve(segment, end);
        if (!crossing && !end.is_integer() && segment.source >= 0) {
            // A line the gap closer extended onto the shape, running along
            // the curve: back to its own end pixel, joined to the curve
            // below.
            const DrawnLine& line = lines[static_cast<std::size_t>(segment.source)];
            const RationalPoint pixel_end(fine_end(from_on ? line.from : line.to));
            if (pixel_end != (from_on ? moved.to : moved.from)) {
                end = pixel_end;
            }
        }
        if (!crossing && end.is_integer()) {
            // An end on the grid (a pixel centre) running along the curve:
            // it stays and a connector joins it to the curve's nearest
            // point on the grid.
            if (const auto connector = to_nearest(IntPoint{.x = end.x_num(), .y = end.y_num()})) {
                result.push_back(moved);
                result.push_back(*connector);
                continue;
            }
        }
        if (!crossing) {
            const Vec2 at = detail::to_picture(end);
            return std::format("the {} ending on the shape at ({:.2f}, {:.2f}) does not reach its curve within 2 px",
                               segment.source >= 0 ? "line" : "connector", at.x / curve_scale, at.y / curve_scale);
        }
        end = *crossing;
        if (moved.from == moved.to) {
            continue;
        }
        if (core::orientation(moved.a, moved.b, moved.from) != 0 ||
            core::orientation(moved.a, moved.b, moved.to) != 0) {
            return std::string("an edge ending on the shape does not reach its curve");
        }
        result.push_back(moved);
    }
    std::ranges::move(chords, std::back_inserter(result));
    return result;
}

// Puts each curved shape into the topology as the chords of its pieces
// (docs/hd-pipeline.md, "Curved shapes in the arrangement"); the topology
// is then in fine units (scale curve_scale). A shape is kept only if the
// partition gets no more mixed faces or 1x-block pixels. Whether any shape
// was put in; why each other one was not, in `why`.
// NOLINTNEXTLINE(readability-function-cognitive-complexity): jointly, then one by one, as for gap hints.
bool enter_curved_shapes(std::vector<TopoSegment>& topology, std::int64_t& scale,
                         const std::vector<CurvedShape>& shapes, const std::vector<DrawnLine>& lines,
                         const std::vector<std::optional<TopoSegment>>& own, Partitioner& partition,
                         std::vector<Entered>& in_arrangement, std::vector<CurveRefusal>& why) {
    std::vector<TopoSegment> fine;
    if (scale == curve_scale) {
        fine = topology;
    } else {
        SCI_ASSERT(scale == 1);
        fine.reserve(topology.size());
        for (const TopoSegment& segment : topology) {
            fine.push_back(scaled(segment, curve_scale));
        }
    }
    why.clear();
    // Segments scaled exactly give the same arrangement, so the fast
    // analysis takes the partition of the doubled grid it made already; the
    // reference makes the one of the fine grid.
    Partition current =
        partition.analysis() == Analysis::Fast ? partition(topology, scale) : partition(fine, curve_scale);
    // All together first (one partition), then one by one.
    if (shapes.size() > 1) {
        std::vector<TopoSegment> all = fine;
        bool complete = true;
        for (const CurvedShape& shape : shapes) {
            auto candidate = with_curve(all, shape, lines, own);
            if (auto* segments = std::get_if<std::vector<TopoSegment>>(&candidate)) {
                all = std::move(*segments);
            } else {
                complete = false;
                break;
            }
        }
        if (complete) {
            if (const Partition with = partition(all, curve_scale); no_worse(with, current)) {
                for (const CurvedShape& shape : shapes) {
                    for (const std::size_t line : shape.lines) {
                        in_arrangement[line] = Entered::Curve;
                    }
                }
                topology = std::move(all);
                scale = curve_scale;
                return true;
            }
        }
    }
    bool entered = false;
    for (const CurvedShape& shape : shapes) {
        auto candidate = with_curve(fine, shape, lines, own);
        if (const auto* reason = std::get_if<std::string>(&candidate)) {
            why.push_back(CurveRefusal{.picture = shape.picture, .id = shape.id, .reason = *reason});
            continue;
        }
        auto& segments = std::get<std::vector<TopoSegment>>(candidate);
        Partition with = partition(segments, curve_scale);
        if (!no_worse(with, current)) {
            std::string where;
            for (const Fallback& fallback : with.stats.fallbacks) {
                if (std::ranges::none_of(current.stats.fallbacks, [&](const Fallback& f) {
                        return f.left == fallback.left && f.top == fallback.top && f.right == fallback.right &&
                               f.bottom == fallback.bottom;
                    })) {
                    where +=
                        std::format(" ({}, {})-({}, {})", fallback.left, fallback.top, fallback.right, fallback.bottom);
                }
            }
            why.push_back(CurveRefusal{
                .picture = shape.picture,
                .id = shape.id,
                .reason = std::format("the curve in the partition adds mixed faces or 1x blocks ({} to {} mixed, {} "
                                      "to {} pixels; new at{})",
                                      current.stats.mixed_faces, with.stats.mixed_faces, current.stats.fallback_pixels,
                                      with.stats.fallback_pixels, where),
            });
            continue;
        }
        fine = std::move(segments);
        current = std::move(with);
        for (const std::size_t line : shape.lines) {
            in_arrangement[line] = Entered::Curve;
        }
        entered = true;
    }
    if (entered) {
        topology = std::move(fine);
        scale = curve_scale;
    }
    return entered;
}

// Adds why a curved shape whose faces keep the 3b fit did not enter the
// arrangement to its report entry.
void note_curves_not_entered(const std::vector<CurveRefusal>& refusals, SceneStats& stats) {
    for (const CurveRefusal& refusal : refusals) {
        for (HintOutcome& outcome : stats.applied_hints) {
            if (outcome.picture == refusal.picture && outcome.id == refusal.id &&
                outcome.detail.contains("the faces keep the 3b fit")) {
                outcome.detail += std::format("; not a curve of the partition: {}", refusal.reason);
            }
        }
    }
}

// The step corners: each stroke runs on to its end pixel's outer edge and
// is cut there.
void square_step_ends(std::vector<Layer>& layers, const std::vector<DrawnLine>& lines, const SceneModelFits& model) {
    for (const auto& [line, step] : model.square_ends) {
        for (Layer& layer : layers) {
            auto* stroke = std::get_if<Stroke>(&layer);
            if (stroke == nullptr || !std::cmp_equal(stroke->source, lines[line].primitive)) {
                continue;
            }
            for (Vec2* end : {&stroke->points.front(), &stroke->points.back()}) {
                if (*end == step.end) {
                    *end = step.edge;
                    stroke->cuts.push_back(step.cut);
                    break;
                }
            }
        }
    }
}

// The lines as the stroke integrity pass sees them: the pixels each drew,
// and the ends the scene model fixed. `decided` holds the primitives of the
// lines the customer's decisions steer.
std::vector<detail::StrokeLine> stroke_lines_of(const std::vector<DrawnLine>& lines, const SceneModelFits& model,
                                                std::span<const std::int32_t> decided) {
    std::vector<detail::StrokeLine> stroke_lines;
    stroke_lines.reserve(lines.size());
    for (const DrawnLine& line : lines) {
        detail::StrokeLine stroke_line{
            .primitive = static_cast<std::int32_t>(line.primitive),
            .path = {},
            .fixed = {},
            .inner = {},
            .decided = false,
        };
        pic::for_each_line_pixel(line.from, line.to, [&](int x, int y) {
            if (y >= picture_top && y < picture_top + picture_height) {
                stroke_line.path.push_back(picture_index(x, y - picture_top));
            }
        });
        stroke_lines.push_back(std::move(stroke_line));
    }
    for (const auto& [line, step] : model.square_ends) {
        stroke_lines[line].fixed.push_back(picture_index(static_cast<int>(step.end.x), static_cast<int>(step.end.y)));
    }
    for (const auto& [line, end] : model.inner_ends) {
        stroke_lines[line].fixed.push_back(picture_index(end.x, end.y - picture_top));
        stroke_lines[line].inner.push_back(picture_index(end.x, end.y - picture_top));
    }
    for (std::size_t line = 0; line < lines.size(); ++line) {
        stroke_lines[line].decided =
            model.steered[line] && std::ranges::find(decided, stroke_lines[line].primitive) != decided.end();
    }
    return stroke_lines;
}

FittedKind fitted_kind(detail::CurveKind kind) {
    switch (kind) {
    case detail::CurveKind::Line:
        return FittedKind::Line;
    case detail::CurveKind::Arc:
        return FittedKind::Arc;
    case detail::CurveKind::Spline:
        return FittedKind::Spline;
    case detail::CurveKind::Ellipse:
        return FittedKind::Ellipse;
    }
    return FittedKind::Spline;
}

// The fitted chains and the strokes of their lines as fitted, for the
// round-trip check (A4 A.6). Changes nothing in the scene.
void record_fits(const detail::Arrangement& arrangement, const Curves& curves, const std::vector<DrawnLine>& lines,
                 const PlaneHints& hints, const SceneModelFits& model, SceneStats& stats) {
    for (const detail::FittedChain& chain : curves.fitted) {
        FittedChain record{.kind = fitted_kind(chain.kind), .hint = {}, .lines = {}};
        if (chain.hint >= 0) {
            record.hint = hints.chains[static_cast<std::size_t>(chain.hint)].hint->id;
        }
        for (const std::int32_t edge : chain.edges) {
            const std::int32_t source = arrangement.sources[static_cast<std::size_t>(edge)];
            // Lines the scene model steers are checked with their primitive.
            if (source >= 0 && !model.steered[static_cast<std::size_t>(source)]) {
                record.lines.push_back(static_cast<std::int32_t>(lines[static_cast<std::size_t>(source)].primitive));
            }
        }
        std::ranges::stable_sort(record.lines);
        const auto [first, last] = std::ranges::unique(record.lines);
        record.lines.erase(first, last);
        if (!record.lines.empty()) {
            stats.fitted_chains.push_back(std::move(record));
        }
    }
    stats.fitted_chains.insert(stats.fitted_chains.end(), model.records.begin(), model.records.end());
    for (std::size_t line = 0; line < lines.size(); ++line) {
        if (!curves.strokes[line].empty()) {
            stats.fitted_strokes.push_back(FittedLine{
                .primitive = static_cast<std::int32_t>(lines[line].primitive),
                .points = curves.strokes[line],
            });
        }
    }
}

// The layers over the faces, in drawing order: visible lines as strokes,
// solid brushes as shapes, textured brushes as blocks.
std::vector<Layer> build_layers(const pic::DrawTrace& trace, const PlaneView& view, Plane plane,
                                const std::vector<DrawnLine>& lines, const std::vector<int>& owned,
                                const std::vector<std::vector<Vec2>>& curves, SceneStats& stats) {
    std::vector<Layer> layers;
    std::size_t next_line = 0;
    for (std::size_t i = 0; i < trace.primitives.size(); ++i) {
        const auto& primitive = trace.primitives[i];
        if (!in_plane(primitive, plane)) {
            continue;
        }
        const std::uint8_t value = value_of(primitive, plane);
        if (std::holds_alternative<pic::Line>(primitive.shape)) {
            const std::size_t index = next_line++;
            const DrawnLine& line = lines[index];
            ++stats.lines;
            if (owned[i] == 0) {
                ++stats.hidden_lines;
                continue;
            }
            Stroke stroke{
                .source = static_cast<std::int32_t>(i),
                .points = {centre(line.from)},
                .value = value,
                .fitted = false,
                .cuts = {},
            };
            if (!curves[index].empty()) {
                stroke.points = curves[index];
                stroke.fitted = true;
            } else if (!line.is_dot()) {
                stroke.points.push_back(centre(line.to));
            }
            layers.emplace_back(std::move(stroke));
            ++stats.strokes;
        } else if (const auto* pattern = std::get_if<pic::Pattern>(&primitive.shape);
                   pattern != nullptr && owned[i] > 0) {
            if (pic::pattern_is_textured(pattern->code)) {
                layers.emplace_back(owned_blocks(view, static_cast<std::int32_t>(i)));
                ++stats.textured;
            } else if (auto brush = brush_layer(*pattern, value, static_cast<std::int32_t>(i))) {
                layers.push_back(std::move(*brush));
                ++stats.brushes;
            }
        }
    }
    return layers;
}

} // namespace

#ifdef SCI_WITHOUT_STROKE_INTEGRITY
constexpr bool without_stroke_integrity = true;
#else
constexpr bool without_stroke_integrity = false;
#endif

// NOLINTNEXTLINE(readability-function-cognitive-complexity): the plane's steps in order, with the entry rounds.
BuiltScene build_scene(const pic::PictureMaps& maps, const pic::DrawTrace& trace, Plane plane,
                       std::span<const PictureHints* const> hints, Analysis analysis) {
    BuiltScene built;
    Scene& scene = built.scene;
    scene.plane = plane;
    const PlaneView view = view_plane(maps, trace, plane);
    const std::vector<DrawnLine> lines = collect_lines(trace, plane);

    // One topology segment per line (dots have none), in line order; its
    // source is the line's index.
    std::vector<std::optional<TopoSegment>> segments;
    segments.reserve(lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].is_dot()) {
            segments.emplace_back();
        } else {
            segments.emplace_back(
                detail::make_segment(doubled(lines[i].from), doubled(lines[i].to), static_cast<std::int32_t>(i)));
        }
    }
    GapCloser closer(lines, segments, built.stats);
    closer.close_all();
    std::vector<TopoSegment> topology;
    for (const auto& segment : segments) {
        if (segment) {
            topology.push_back(*segment);
        }
    }
    topology.insert(topology.end(), closer.connectors().begin(), closer.connectors().end());
    const std::vector<TopoSegment> frame = picture_frame();
    topology.insert(topology.end(), frame.begin(), frame.end());

    const PlaneHints plane_hints = resolve_hints(trace, lines, hints, built.stats);
    const std::vector<int> owned = owned_pixels(trace, view);
    Partitioner partition(view, trace, analysis);
    // A straight steered shape the partition cannot take by moving its
    // vertices enters the arrangement as one segment, and the plane is
    // built again (docs/hd-pipeline.md, "The shared boundary"); a curved
    // one enters as chords on the fine grid ("Curved shapes in the
    // arrangement"). A shape entered can make a neighbour fail to share, so
    // that one may enter in a later round; at most max_entry_rounds.
    constexpr int max_entry_rounds = 3;
    const SceneStats before_steering = built.stats;
    std::vector<Entered> in_arrangement(lines.size(), Entered::No);
    std::int64_t scale = 1;
    std::vector<CurveRefusal> curve_why;
    for (int round = 0;; ++round) {
        const bool again = plane == Plane::Visual && round < max_entry_rounds;
        Partition parts = close_hinted_gaps(topology, scale, plane_hints, lines, partition, built.stats);
        std::vector<bool> curve_lines(lines.size(), false);
        for (std::size_t line = 0; line < lines.size(); ++line) {
            curve_lines[line] = in_arrangement[line] == Entered::Curve;
        }
        Curves curves =
            fit_with_hints(parts.arrangement, lines, segments, owned, view, plane_hints, curve_lines, built.stats);
        // The scene model changes the HD picture only, so the visual plane;
        // a steered shape is the faces' boundary as well as the strokes.
        SceneModelFits model{
            .steered = std::vector<bool>(lines.size(), false),
            .straight = {},
            .curved = {},
            .records = {},
            .square_ends = {},
            .inner_ends = {},
        };
        detail::SharedBoundary boundary(parts.arrangement, curves.fitted, analysis);
        if (plane == Plane::Visual) {
            model = steer_by_scene_model(trace, lines, view, hints, in_arrangement, curves, boundary, built.stats);
        }
        if (again) {
            // Straight shapes enter the doubled grid, in the first round.
            const bool straight = scale == 1 && !model.straight.empty() &&
                                  enter_straight_shapes(topology, model.straight, lines, partition, in_arrangement);
            const bool curved =
                !model.curved.empty() && enter_curved_shapes(topology, scale, model.curved, lines, segments, partition,
                                                             in_arrangement, curve_why);
            if (straight || curved) {
                built.stats = before_steering;
                continue;
            }
        }
        note_curves_not_entered(curve_why, built.stats);
        copy_face_stats(parts.stats, built.stats);
        scene.faces = std::move(parts.faces);
        scene.block_values = block_values(view, trace);
        record_fits(parts.arrangement, curves, lines, plane_hints, model, built.stats);
        scene.edges =
            scan_edges(parts.arrangement, curves.fitted, boundary.changed() ? &boundary.overrides() : nullptr);
        scene.layers = build_layers(trace, view, plane, lines, owned, curves.strokes, built.stats);
        square_step_ends(scene.layers, lines, model);
        // Stroke integrity and occlusion (spec section 6.10) change the HD
        // picture only, so the visual plane.
        // The baseline of milestone 3b's report is measured without the pass
        // (development option SCI_WITHOUT_STROKE_INTEGRITY).
        if (plane == Plane::Visual && !without_stroke_integrity) {
            const std::vector<detail::StrokeLine> stroke_lines =
                stroke_lines_of(lines, model, built.stats.decided_lines);
            detail::apply_stroke_integrity(scene.layers, stroke_lines, view.owners, built.stats);
        }
        return built;
    }
}

} // namespace sci::vector

#ifdef SCI_DEBUG_HIGHLIGHT
namespace sci::vector {

Scene highlight_replacements(Scene scene) {
    constexpr std::uint8_t fitted = 0xDD;   // Light magenta.
    constexpr std::uint8_t fallback = 0xEE; // Yellow.
    for (Layer& layer : scene.layers) {
        if (auto* stroke = std::get_if<Stroke>(&layer); stroke != nullptr && stroke->fitted) {
            stroke->value = fitted;
        }
    }
    // Only fallback faces read the block values.
    std::ranges::fill(scene.block_values, fallback);
    return scene;
}

} // namespace sci::vector
#endif
