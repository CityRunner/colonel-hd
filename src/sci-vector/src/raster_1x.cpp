// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/vector/raster_1x.hpp"

#include "sci/pic/picture_maps.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>

namespace sci::vector {

namespace {

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

void normalise(std::vector<std::uint32_t>& pixels) {
    std::ranges::stable_sort(pixels);
    const auto [first, last] = std::ranges::unique(pixels);
    pixels.erase(first, last);
}

int one_way(std::span<const std::uint32_t> from, std::span<const std::uint32_t> to) {
    int worst = 0;
    for (const std::uint32_t p : from) {
        if (std::ranges::binary_search(to, p)) {
            continue;
        }
        int nearest = to.empty() ? picture_width : std::numeric_limits<int>::max();
        for (const std::uint32_t q : to) {
            nearest = std::min(nearest, chebyshev(pixel_of(p), pixel_of(q)));
        }
        worst = std::max(worst, nearest);
    }
    return worst;
}

} // namespace

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

std::vector<std::uint32_t> line_pixels_1x(pic::ScreenPoint from, pic::ScreenPoint to) {
    std::vector<std::uint32_t> pixels;
    pic::for_each_line_pixel(from, to, [&](int x, int y) {
        const int py = y - pic::picture_top;
        if (py >= 0 && py < picture_height && x >= 0 && x < picture_width) {
            pixels.push_back(index_of(x, py));
        }
    });
    normalise(pixels);
    return pixels;
}

int raster_deviation(std::span<const std::uint32_t> a, std::span<const std::uint32_t> b) {
    return std::max(one_way(a, b), one_way(b, a));
}

bool within_round_trip_tolerance(std::span<const Vec2> points, int x, int y) {
    const Pixel target{.x = x, .y = y};
    return std::ranges::any_of(rasterize_1x(points),
                               [&](std::uint32_t pixel) { return chebyshev(pixel_of(pixel), target) <= 1; });
}

} // namespace sci::vector
