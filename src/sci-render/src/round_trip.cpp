// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/render/round_trip.hpp"

#include "sci/pic/geometry.hpp"
#include "sci/pic/picture_maps.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
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

int snap(double v, int size) {
    const double floor = std::floor(v);
    if (!(floor >= 0)) {
        return 0;
    }
    if (floor >= size - 1) {
        return size - 1;
    }
    return static_cast<int>(floor);
}

int chebyshev(Pixel a, Pixel b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

std::uint32_t index_of(int x, int y) {
    return static_cast<std::uint32_t>((y * picture_width) + x);
}

Pixel pixel_of(std::uint32_t index) {
    return {.x = static_cast<int>(index % picture_width), .y = static_cast<int>(index / picture_width)};
}

// The pixels of a set (sorted indices) that are not in the other, with the
// distance to the nearest pixel of the other.
struct Deviation {
    int count = 0;
    int max = 0;
    std::int64_t sum = 0;
    std::uint32_t worst = 0;
};

Deviation deviation(const std::vector<std::uint32_t>& from, const std::vector<std::uint32_t>& to) {
    Deviation result;
    for (const std::uint32_t p : from) {
        if (std::ranges::binary_search(to, p)) {
            continue;
        }
        const Pixel a = pixel_of(p);
        int nearest = std::numeric_limits<int>::max();
        for (const std::uint32_t q : to) {
            nearest = std::min(nearest, chebyshev(a, pixel_of(q)));
        }
        if (to.empty()) {
            nearest = picture_width;
        }
        ++result.count;
        result.sum += nearest;
        if (nearest > result.max) {
            result.max = nearest;
            result.worst = p;
        }
    }
    return result;
}

// Sorted, without duplicates.
void normalise(std::vector<std::uint32_t>& pixels) {
    std::ranges::stable_sort(pixels);
    const auto [first, last] = std::ranges::unique(pixels);
    pixels.erase(first, last);
}

// The pixels a PIC line drew within the picture area.
std::vector<std::uint32_t> line_pixels(const pic::Line& line) {
    std::vector<std::uint32_t> pixels;
    const auto to_screen = [](pic::Point p) { return pic::ScreenPoint{.x = p.x, .y = p.y + pic::picture_top}; };
    pic::for_each_line_pixel(to_screen(line.from), to_screen(line.to), [&](int x, int y) {
        const int py = y - pic::picture_top;
        if (py >= 0 && py < picture_height && x >= 0 && x < picture_width) {
            pixels.push_back(index_of(x, py));
        }
    });
    normalise(pixels);
    return pixels;
}

} // namespace

std::int64_t RoundTripChain::mean_milli() const {
    const std::int64_t pixels = std::int64_t{original_pixels} + reconstructed_pixels;
    return pixels == 0 ? 0 : ((distance_sum * 2000) + pixels) / (2 * pixels);
}

std::vector<std::uint32_t> rasterize_1x(std::span<const Vec2> points) {
    // The pixels the points lie in, with staircase corners dropped.
    std::vector<Pixel> knots;
    for (const Vec2 p : points) {
        const Pixel pixel{.x = snap(p.x, picture_width), .y = snap(p.y, picture_height)};
        if (!knots.empty() && knots.back() == pixel) {
            continue;
        }
        while (knots.size() >= 2 && chebyshev(knots[knots.size() - 2], pixel) <= 1) {
            knots.pop_back();
        }
        if (knots.empty() || knots.back() != pixel) {
            knots.push_back(pixel);
        }
    }
    std::vector<std::uint32_t> pixels;
    const auto visit = [&](int x, int y) {
        const int py = y - pic::picture_top;
        if (py >= 0 && py < picture_height) {
            pixels.push_back(index_of(x, py));
        }
    };
    const auto screen = [](Pixel p) { return pic::ScreenPoint{.x = p.x, .y = p.y + pic::picture_top}; };
    if (knots.size() == 1) {
        visit(knots[0].x, knots[0].y + pic::picture_top);
    }
    for (std::size_t i = 1; i < knots.size(); ++i) {
        pic::for_each_line_pixel(screen(knots[i - 1]), screen(knots[i]), visit);
    }
    normalise(pixels);
    return pixels;
}

namespace {

// Pixels with the line they belong to: per pixel, the first of the chain's
// lines that drew it (original) or whose stroke covers it (reconstruction).
struct TaggedPixels {
    std::vector<std::pair<std::uint32_t, std::int32_t>> tagged;

    // Sorts the tags by pixel (stable: the first line stays first) and
    // returns the pixels, sorted and without duplicates.
    std::vector<std::uint32_t> pixels() {
        std::ranges::stable_sort(tagged, {}, &std::pair<std::uint32_t, std::int32_t>::first);
        std::vector<std::uint32_t> pixels;
        pixels.reserve(tagged.size());
        for (const auto& [p, line] : tagged) {
            pixels.push_back(p);
        }
        normalise(pixels);
        return pixels;
    }

    // After pixels(): the line of a pixel.
    [[nodiscard]] std::int32_t line_at(std::uint32_t p) const {
        const auto it = std::ranges::lower_bound(tagged, p, {}, &std::pair<std::uint32_t, std::int32_t>::first);
        return it == tagged.end() ? std::int32_t{-1} : it->second;
    }
};

// Missed pixels that one of the chain's lines owns at 1x.
int missed_seen(const pic::DrawTrace& trace, const vector::FittedChain& chain,
                const std::vector<std::uint32_t>& original, const std::vector<std::uint32_t>& reconstructed) {
    int seen = 0;
    for (const std::uint32_t p : original) {
        if (std::ranges::binary_search(reconstructed, p)) {
            continue;
        }
        const Pixel px = pixel_of(p);
        const std::int32_t owner = trace.visual_owner[pic::PictureMaps::index(px.x, px.y + pic::picture_top)];
        if (std::ranges::binary_search(chain.lines, owner)) {
            ++seen;
        }
    }
    return seen;
}

RoundTripChain check_chain(const pic::DrawTrace& trace, const vector::FittedChain& chain,
                           const std::map<std::int32_t, const vector::FittedLine*>& fitted) {
    RoundTripChain check;
    check.kind = chain.kind;
    check.hinted = !chain.hint.empty();
    check.first_line = chain.lines.empty() ? -1 : chain.lines.front();
    TaggedPixels original_of;
    TaggedPixels reconstructed_of;
    for (const std::int32_t primitive : chain.lines) {
        const auto& traced = trace.primitives[static_cast<std::size_t>(primitive)];
        if (const auto* line = std::get_if<pic::Line>(&traced.shape)) {
            for (const std::uint32_t p : line_pixels(*line)) {
                original_of.tagged.emplace_back(p, primitive);
            }
        }
        if (const auto found = fitted.find(primitive); found != fitted.end()) {
            for (const std::uint32_t p : rasterize_1x(found->second->points)) {
                reconstructed_of.tagged.emplace_back(p, primitive);
            }
        }
    }
    const std::vector<std::uint32_t> original = original_of.pixels();
    const std::vector<std::uint32_t> reconstructed = reconstructed_of.pixels();
    const Deviation added = deviation(reconstructed, original);
    const Deviation missed = deviation(original, reconstructed);
    check.original_pixels = static_cast<int>(original.size());
    check.reconstructed_pixels = static_cast<int>(reconstructed.size());
    check.added = added.count;
    check.missed = missed.count;
    check.missed_seen = missed_seen(trace, chain, original, reconstructed);
    check.max_distance = std::max(added.max, missed.max);
    check.distance_sum = added.sum + missed.sum;
    if (check.max_distance > 0) {
        // The first pixel in raster order with the largest distance.
        const bool from_added =
            added.max == check.max_distance && (missed.max < added.max || added.worst <= missed.worst);
        const std::uint32_t worst = from_added ? added.worst : missed.worst;
        check.worst_x = pixel_of(worst).x;
        check.worst_y = pixel_of(worst).y;
        check.worst_line = from_added ? reconstructed_of.line_at(worst) : original_of.line_at(worst);
    }
    return check;
}

} // namespace

RoundTrip check_round_trip(const pic::DrawTrace& trace, const vector::SceneStats& stats) {
    std::map<std::int32_t, const vector::FittedLine*> fitted;
    for (const vector::FittedLine& line : stats.fitted_strokes) {
        fitted.emplace(line.primitive, &line);
    }
    RoundTrip result;
    for (std::size_t c = 0; c < stats.fitted_chains.size(); ++c) {
        RoundTripChain check = check_chain(trace, stats.fitted_chains[c], fitted);
        check.chain = c;
        if (check.passes()) {
            ++result.passing;
        } else {
            ++result.failing;
        }
        result.chains.push_back(check);
    }
    return result;
}

} // namespace sci::render
