// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/window_scale.hpp"

#include "sci/platform/config.hpp"
#include "sci/render/compose.hpp"

#include <algorithm>

namespace sci::platform {

namespace {

// The largest N that fits, at least 1 and not capped.
int largest_fit(int width, int height) {
    return std::max(1, std::min(width / render::compose_width, height / render::compose_height));
}

} // namespace

int fit_scale(int width, int height) {
    return std::min(largest_fit(width, height), max_scale);
}

int scale_for_window(int configured, int width, int height) {
    return configured > 0 ? std::min(configured, largest_fit(width, height)) : fit_scale(width, height);
}

int initial_window_scale(int configured, std::optional<DisplaySize> usable) {
    constexpr int auto_scale = 3;
    if (configured <= 0) {
        return auto_scale;
    }
    if (!usable) {
        return configured;
    }
    return std::min(configured, largest_fit(usable->width, usable->height));
}

} // namespace sci::platform
